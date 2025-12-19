import math
from datetime import datetime
from fastapi import APIRouter, Depends, HTTPException
from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy.orm import selectinload

from ..database import get_db
from ..models import Firmware, OTAUpdate, OTANodeStatus
from ..schemas import (
    OTAUpdateCreate, OTAUpdateOut, OTAUpdateStatus,
    OTAPendingUpdate, OTANodeStatusOut, OTAProgressReport
)

router = APIRouter(prefix="/api/v1/ota", tags=["ota"])

OTA_PART_SIZE = 1024  # Bytes per chunk for painlessMesh OTA


@router.post("/updates", response_model=OTAUpdateOut)
async def create_update(
    update: OTAUpdateCreate,
    db: AsyncSession = Depends(get_db),
):
    """Create an OTA update job."""
    # Validate firmware exists
    result = await db.execute(select(Firmware).where(Firmware.id == update.firmware_id))
    firmware = result.scalar_one_or_none()
    if not firmware:
        raise HTTPException(status_code=404, detail="Firmware not found")

    # Create update job
    ota_update = OTAUpdate(
        firmware_id=update.firmware_id,
        target_node_id=update.target_node_id,
        target_node_type=update.target_node_type or firmware.node_type,
        force_update=update.force_update,
    )

    db.add(ota_update)
    await db.commit()
    await db.refresh(ota_update)

    return OTAUpdateOut(
        id=ota_update.id,
        firmware_id=ota_update.firmware_id,
        target_node_id=ota_update.target_node_id,
        target_node_type=ota_update.target_node_type,
        status=ota_update.status,
        force_update=ota_update.force_update,
        created_at=ota_update.created_at,
        started_at=ota_update.started_at,
        completed_at=ota_update.completed_at,
    )


@router.get("/updates", response_model=list[OTAUpdateOut])
async def list_updates(
    status: str | None = None,
    db: AsyncSession = Depends(get_db),
):
    """List all OTA update jobs."""
    query = select(OTAUpdate).order_by(OTAUpdate.created_at.desc())
    if status:
        query = query.where(OTAUpdate.status == status)

    result = await db.execute(query)
    updates = result.scalars().all()

    return [
        OTAUpdateOut(
            id=u.id,
            firmware_id=u.firmware_id,
            target_node_id=u.target_node_id,
            target_node_type=u.target_node_type,
            status=u.status,
            force_update=u.force_update,
            created_at=u.created_at,
            started_at=u.started_at,
            completed_at=u.completed_at,
        )
        for u in updates
    ]


@router.get("/updates/pending", response_model=list[OTAPendingUpdate])
async def get_pending_updates(db: AsyncSession = Depends(get_db)):
    """Gateway polls this endpoint for pending updates."""
    result = await db.execute(
        select(OTAUpdate, Firmware)
        .join(Firmware)
        .where(OTAUpdate.status == "pending")
        .order_by(OTAUpdate.created_at)
    )

    updates = []
    for update, firmware in result.all():
        num_parts = math.ceil(firmware.size_bytes / OTA_PART_SIZE)
        updates.append(
            OTAPendingUpdate(
                update_id=update.id,
                firmware_id=firmware.id,
                node_type=firmware.node_type,
                version=firmware.version,
                hardware=firmware.hardware,
                md5=firmware.md5_hash,
                num_parts=num_parts,
                size_bytes=firmware.size_bytes,
                target_node_id=update.target_node_id,
                force=update.force_update,
            )
        )

    return updates


@router.get("/updates/{update_id}", response_model=OTAUpdateStatus)
async def get_update_status(update_id: int, db: AsyncSession = Depends(get_db)):
    """Get detailed status of an OTA update including per-node progress."""
    result = await db.execute(
        select(OTAUpdate)
        .options(selectinload(OTAUpdate.node_statuses), selectinload(OTAUpdate.firmware))
        .where(OTAUpdate.id == update_id)
    )
    update = result.scalar_one_or_none()

    if not update:
        raise HTTPException(status_code=404, detail="Update not found")

    return OTAUpdateStatus(
        id=update.id,
        firmware_id=update.firmware_id,
        node_type=update.firmware.node_type,
        version=update.firmware.version,
        status=update.status,
        force_update=update.force_update,
        created_at=update.created_at,
        started_at=update.started_at,
        completed_at=update.completed_at,
        nodes=[
            OTANodeStatusOut(
                node_id=ns.node_id,
                status=ns.status,
                current_part=ns.current_part,
                total_parts=ns.total_parts,
                error_message=ns.error_message,
                started_at=ns.started_at,
                completed_at=ns.completed_at,
            )
            for ns in update.node_statuses
        ],
    )


@router.post("/updates/{update_id}/start")
async def start_update(update_id: int, db: AsyncSession = Depends(get_db)):
    """Gateway calls this when it starts distributing an update."""
    result = await db.execute(select(OTAUpdate).where(OTAUpdate.id == update_id))
    update = result.scalar_one_or_none()

    if not update:
        raise HTTPException(status_code=404, detail="Update not found")

    if update.status != "pending":
        raise HTTPException(status_code=400, detail=f"Update is not pending (status: {update.status})")

    update.status = "distributing"
    update.started_at = datetime.utcnow()
    await db.commit()

    return {"status": "distributing", "update_id": update_id}


@router.post("/updates/{update_id}/node/{node_id}/progress")
async def report_node_progress(
    update_id: int,
    node_id: str,
    progress: OTAProgressReport,
    db: AsyncSession = Depends(get_db),
):
    """Gateway reports progress for a specific node."""
    # Verify update exists
    update_result = await db.execute(select(OTAUpdate).where(OTAUpdate.id == update_id))
    update = update_result.scalar_one_or_none()
    if not update:
        raise HTTPException(status_code=404, detail="Update not found")

    # Get or create node status
    result = await db.execute(
        select(OTANodeStatus).where(
            OTANodeStatus.update_id == update_id,
            OTANodeStatus.node_id == node_id,
        )
    )
    node_status = result.scalar_one_or_none()

    if not node_status:
        node_status = OTANodeStatus(
            update_id=update_id,
            node_id=node_id,
            started_at=datetime.utcnow(),
        )
        db.add(node_status)

    node_status.status = progress.status
    node_status.current_part = progress.current_part
    node_status.total_parts = progress.total_parts
    node_status.error_message = progress.error_message

    if progress.status == "completed":
        node_status.completed_at = datetime.utcnow()
    elif progress.status == "failed":
        node_status.completed_at = datetime.utcnow()

    await db.commit()

    return {"status": "ok", "node_id": node_id, "progress": f"{progress.current_part}/{progress.total_parts}"}


@router.post("/updates/{update_id}/complete")
async def complete_update(update_id: int, db: AsyncSession = Depends(get_db)):
    """Gateway calls this when update distribution is complete."""
    result = await db.execute(select(OTAUpdate).where(OTAUpdate.id == update_id))
    update = result.scalar_one_or_none()

    if not update:
        raise HTTPException(status_code=404, detail="Update not found")

    update.status = "completed"
    update.completed_at = datetime.utcnow()
    await db.commit()

    return {"status": "completed", "update_id": update_id}


@router.post("/updates/{update_id}/fail")
async def fail_update(
    update_id: int,
    error_message: str = "Unknown error",
    db: AsyncSession = Depends(get_db),
):
    """Mark an update as failed."""
    result = await db.execute(select(OTAUpdate).where(OTAUpdate.id == update_id))
    update = result.scalar_one_or_none()

    if not update:
        raise HTTPException(status_code=404, detail="Update not found")

    update.status = "failed"
    update.completed_at = datetime.utcnow()
    await db.commit()

    return {"status": "failed", "update_id": update_id, "error": error_message}


@router.delete("/updates/{update_id}")
async def cancel_update(update_id: int, db: AsyncSession = Depends(get_db)):
    """Cancel/delete a pending update."""
    result = await db.execute(select(OTAUpdate).where(OTAUpdate.id == update_id))
    update = result.scalar_one_or_none()

    if not update:
        raise HTTPException(status_code=404, detail="Update not found")

    if update.status == "distributing":
        raise HTTPException(status_code=400, detail="Cannot cancel update in progress")

    await db.delete(update)
    await db.commit()

    return {"message": f"Update {update_id} cancelled"}
