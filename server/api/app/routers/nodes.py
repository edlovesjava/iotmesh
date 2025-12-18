from fastapi import APIRouter, Depends, HTTPException
from sqlalchemy import select, delete
from sqlalchemy.ext.asyncio import AsyncSession

from ..database import get_db
from ..models import Node, Telemetry
from ..schemas import NodeOut, NodeUpdate

router = APIRouter(prefix="/api/v1/nodes", tags=["nodes"])


@router.get("", response_model=list[NodeOut])
async def list_nodes(db: AsyncSession = Depends(get_db)):
    """List all registered nodes."""
    result = await db.execute(select(Node).order_by(Node.last_seen.desc()))
    nodes = result.scalars().all()

    # Get latest peer_count for each node from telemetry
    nodes_out = []
    for node in nodes:
        telem_result = await db.execute(
            select(Telemetry.peer_count)
            .where(Telemetry.node_id == node.id)
            .order_by(Telemetry.time.desc())
            .limit(1)
        )
        peer_count = telem_result.scalar()

        node_dict = {
            "id": node.id,
            "name": node.name,
            "firmware_version": node.firmware_version,
            "ip_address": node.ip_address,
            "first_seen": node.first_seen,
            "last_seen": node.last_seen,
            "is_online": node.is_online,
            "role": node.role,
            "peer_count": peer_count,
        }
        nodes_out.append(NodeOut(**node_dict))

    return nodes_out


@router.get("/{node_id}", response_model=NodeOut)
async def get_node(node_id: str, db: AsyncSession = Depends(get_db)):
    """Get details for a specific node."""
    result = await db.execute(select(Node).where(Node.id == node_id))
    node = result.scalar_one_or_none()

    if not node:
        raise HTTPException(status_code=404, detail="Node not found")

    # Get latest peer_count
    telem_result = await db.execute(
        select(Telemetry.peer_count)
        .where(Telemetry.node_id == node.id)
        .order_by(Telemetry.time.desc())
        .limit(1)
    )
    peer_count = telem_result.scalar()

    return NodeOut(
        id=node.id,
        name=node.name,
        firmware_version=node.firmware_version,
        ip_address=node.ip_address,
        first_seen=node.first_seen,
        last_seen=node.last_seen,
        is_online=node.is_online,
        role=node.role,
        peer_count=peer_count,
    )


@router.delete("/{node_id}")
async def delete_node(node_id: str, db: AsyncSession = Depends(get_db)):
    """Remove a node and all its data."""
    result = await db.execute(select(Node).where(Node.id == node_id))
    node = result.scalar_one_or_none()

    if not node:
        raise HTTPException(status_code=404, detail="Node not found")

    await db.execute(delete(Node).where(Node.id == node_id))
    await db.commit()

    return {"message": f"Node {node_id} deleted"}


@router.put("/{node_id}/name", response_model=NodeOut)
async def update_node_name(node_id: str, update: NodeUpdate, db: AsyncSession = Depends(get_db)):
    """Update a node's display name."""
    result = await db.execute(select(Node).where(Node.id == node_id))
    node = result.scalar_one_or_none()

    if not node:
        raise HTTPException(status_code=404, detail="Node not found")

    node.name = update.name
    await db.commit()
    await db.refresh(node)

    return NodeOut(
        id=node.id,
        name=node.name,
        firmware_version=node.firmware_version,
        ip_address=node.ip_address,
        first_seen=node.first_seen,
        last_seen=node.last_seen,
        is_online=node.is_online,
        role=node.role,
        peer_count=None,
    )
