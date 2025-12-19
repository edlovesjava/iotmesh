from fastapi import APIRouter, Depends, HTTPException
from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession

from ..database import get_db
from ..models import Node, CurrentState
from ..schemas import StateOut

router = APIRouter(prefix="/api/v1", tags=["state"])


@router.get("/state", response_model=list[StateOut])
async def get_all_state(db: AsyncSession = Depends(get_db)):
    """Get current state from all nodes."""
    result = await db.execute(
        select(CurrentState).order_by(CurrentState.node_id, CurrentState.key)
    )
    states = result.scalars().all()
    return [StateOut.model_validate(s) for s in states]


@router.get("/nodes/{node_id}/state", response_model=list[StateOut])
async def get_node_state(node_id: str, db: AsyncSession = Depends(get_db)):
    """Get current state for a specific node."""
    # Check node exists
    node_result = await db.execute(select(Node).where(Node.id == node_id))
    if not node_result.scalar_one_or_none():
        raise HTTPException(status_code=404, detail="Node not found")

    result = await db.execute(
        select(CurrentState)
        .where(CurrentState.node_id == node_id)
        .order_by(CurrentState.key)
    )
    states = result.scalars().all()
    return [StateOut.model_validate(s) for s in states]
