import hashlib
from fastapi import APIRouter, Depends, HTTPException, UploadFile, File, Form
from fastapi.responses import Response
from sqlalchemy import select, func
from sqlalchemy.ext.asyncio import AsyncSession

from ..database import get_db
from ..models import Firmware
from ..schemas import FirmwareOut, FirmwareList

router = APIRouter(prefix="/api/v1/firmware", tags=["firmware"])


@router.post("/upload", response_model=FirmwareOut)
async def upload_firmware(
    file: UploadFile = File(...),
    node_type: str = Form(...),
    version: str = Form(...),
    hardware: str = Form("ESP32"),
    release_notes: str = Form(None),
    is_stable: bool = Form(False),
    db: AsyncSession = Depends(get_db),
):
    """Upload a firmware binary file."""
    # Read the binary data
    binary_data = await file.read()
    size_bytes = len(binary_data)

    # Calculate MD5 hash
    md5_hash = hashlib.md5(binary_data).hexdigest()

    # Check for existing firmware with same node_type, version, hardware
    existing = await db.execute(
        select(Firmware).where(
            Firmware.node_type == node_type,
            Firmware.version == version,
            Firmware.hardware == hardware,
        )
    )
    if existing.scalar_one_or_none():
        raise HTTPException(
            status_code=409,
            detail=f"Firmware already exists for {node_type} v{version} ({hardware})"
        )

    # Create firmware record
    firmware = Firmware(
        node_type=node_type,
        version=version,
        hardware=hardware,
        filename=file.filename or f"{node_type}_{version}.bin",
        size_bytes=size_bytes,
        md5_hash=md5_hash,
        binary_data=binary_data,
        release_notes=release_notes,
        is_stable=is_stable,
    )

    db.add(firmware)
    await db.commit()
    await db.refresh(firmware)

    return FirmwareOut(
        id=firmware.id,
        node_type=firmware.node_type,
        version=firmware.version,
        hardware=firmware.hardware,
        filename=firmware.filename,
        size_bytes=firmware.size_bytes,
        md5_hash=firmware.md5_hash,
        release_notes=firmware.release_notes,
        is_stable=firmware.is_stable,
        created_at=firmware.created_at,
    )


@router.get("", response_model=FirmwareList)
async def list_firmware(
    node_type: str | None = None,
    db: AsyncSession = Depends(get_db),
):
    """List all firmware versions, optionally filtered by node type."""
    query = select(Firmware).order_by(Firmware.node_type, Firmware.version.desc())

    if node_type:
        query = query.where(Firmware.node_type == node_type)

    result = await db.execute(query)
    firmware_list = result.scalars().all()

    # Get total count
    count_query = select(func.count(Firmware.id))
    if node_type:
        count_query = count_query.where(Firmware.node_type == node_type)
    total = (await db.execute(count_query)).scalar() or 0

    return FirmwareList(
        items=[
            FirmwareOut(
                id=fw.id,
                node_type=fw.node_type,
                version=fw.version,
                hardware=fw.hardware,
                filename=fw.filename,
                size_bytes=fw.size_bytes,
                md5_hash=fw.md5_hash,
                release_notes=fw.release_notes,
                is_stable=fw.is_stable,
                created_at=fw.created_at,
            )
            for fw in firmware_list
        ],
        total=total,
    )


@router.get("/{firmware_id}", response_model=FirmwareOut)
async def get_firmware(firmware_id: int, db: AsyncSession = Depends(get_db)):
    """Get firmware metadata by ID."""
    result = await db.execute(select(Firmware).where(Firmware.id == firmware_id))
    firmware = result.scalar_one_or_none()

    if not firmware:
        raise HTTPException(status_code=404, detail="Firmware not found")

    return FirmwareOut(
        id=firmware.id,
        node_type=firmware.node_type,
        version=firmware.version,
        hardware=firmware.hardware,
        filename=firmware.filename,
        size_bytes=firmware.size_bytes,
        md5_hash=firmware.md5_hash,
        release_notes=firmware.release_notes,
        is_stable=firmware.is_stable,
        created_at=firmware.created_at,
    )


@router.get("/{firmware_id}/download")
async def download_firmware(firmware_id: int, db: AsyncSession = Depends(get_db)):
    """Download firmware binary file."""
    result = await db.execute(select(Firmware).where(Firmware.id == firmware_id))
    firmware = result.scalar_one_or_none()

    if not firmware:
        raise HTTPException(status_code=404, detail="Firmware not found")

    return Response(
        content=firmware.binary_data,
        media_type="application/octet-stream",
        headers={
            "Content-Disposition": f'attachment; filename="{firmware.filename}"',
            "Content-Length": str(firmware.size_bytes),
            "X-MD5": firmware.md5_hash,
        },
    )


@router.delete("/{firmware_id}")
async def delete_firmware(firmware_id: int, db: AsyncSession = Depends(get_db)):
    """Delete a firmware entry."""
    result = await db.execute(select(Firmware).where(Firmware.id == firmware_id))
    firmware = result.scalar_one_or_none()

    if not firmware:
        raise HTTPException(status_code=404, detail="Firmware not found")

    await db.delete(firmware)
    await db.commit()

    return {"message": f"Firmware {firmware_id} deleted", "filename": firmware.filename}


@router.patch("/{firmware_id}/stable")
async def mark_firmware_stable(
    firmware_id: int,
    is_stable: bool = True,
    db: AsyncSession = Depends(get_db),
):
    """Mark or unmark firmware as stable release."""
    result = await db.execute(select(Firmware).where(Firmware.id == firmware_id))
    firmware = result.scalar_one_or_none()

    if not firmware:
        raise HTTPException(status_code=404, detail="Firmware not found")

    firmware.is_stable = is_stable
    await db.commit()
    await db.refresh(firmware)

    return FirmwareOut(
        id=firmware.id,
        node_type=firmware.node_type,
        version=firmware.version,
        hardware=firmware.hardware,
        filename=firmware.filename,
        size_bytes=firmware.size_bytes,
        md5_hash=firmware.md5_hash,
        release_notes=firmware.release_notes,
        is_stable=firmware.is_stable,
        created_at=firmware.created_at,
    )
