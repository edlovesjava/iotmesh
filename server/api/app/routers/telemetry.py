from datetime import datetime, timedelta
from fastapi import APIRouter, Depends, Query
from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession

from ..database import get_db
from ..models import Node, Telemetry, CurrentState, StateHistory
from ..schemas import TelemetryIn, TelemetryOut

router = APIRouter(prefix="/api/v1/nodes", tags=["telemetry"])


@router.post("/{node_id}/telemetry")
async def push_telemetry(node_id: str, data: TelemetryIn, db: AsyncSession = Depends(get_db)):
    """
    Push telemetry data from a node.
    Auto-registers the node if it doesn't exist.
    """
    print(f"[TELEMETRY] node_id={node_id} payload={data.model_dump()}")
    now = datetime.utcnow()

    # Get or create node
    result = await db.execute(select(Node).where(Node.id == node_id))
    node = result.scalar_one_or_none()

    if node:
        # Update existing node
        node.last_seen = now
        node.is_online = True
        if data.name:
            node.name = data.name
        if data.firmware:
            node.firmware_version = data.firmware
        if data.role:
            node.role = data.role
    else:
        # Create new node
        node = Node(
            id=node_id,
            name=data.name,
            firmware_version=data.firmware,
            role=data.role or "NODE",
            first_seen=now,
            last_seen=now,
            is_online=True,
        )
        db.add(node)

    # Store telemetry
    telemetry = Telemetry(
        time=now,
        node_id=node_id,
        heap_free=data.heap_free,
        uptime_sec=data.uptime,
        peer_count=data.peer_count,
        role=data.role,
    )
    db.add(telemetry)

    # Update state if provided
    if data.state:
        for key, value in data.state.items():
            # Check if state exists
            state_result = await db.execute(
                select(CurrentState).where(
                    CurrentState.node_id == node_id,
                    CurrentState.key == key
                )
            )
            current_state = state_result.scalar_one_or_none()

            if current_state:
                # Only update if value changed
                if current_state.value != value:
                    old_version = current_state.version
                    current_state.value = value
                    current_state.version = old_version + 1
                    current_state.updated_at = now

                    # Record in history
                    history = StateHistory(
                        time=now,
                        node_id=node_id,
                        key=key,
                        value=value,
                        version=current_state.version,
                    )
                    db.add(history)
            else:
                # Create new state entry
                new_state = CurrentState(
                    node_id=node_id,
                    key=key,
                    value=value,
                    version=1,
                    updated_at=now,
                )
                db.add(new_state)

                # Record in history
                history = StateHistory(
                    time=now,
                    node_id=node_id,
                    key=key,
                    value=value,
                    version=1,
                )
                db.add(history)

    await db.commit()

    return {"status": "ok", "node_id": node_id, "timestamp": now.isoformat()}


@router.get("/{node_id}/history", response_model=list[TelemetryOut])
async def get_node_history(
    node_id: str,
    hours: int = Query(default=24, ge=1, le=168),
    db: AsyncSession = Depends(get_db)
):
    """Get historical telemetry for a node."""
    since = datetime.utcnow() - timedelta(hours=hours)

    result = await db.execute(
        select(Telemetry)
        .where(Telemetry.node_id == node_id, Telemetry.time >= since)
        .order_by(Telemetry.time.desc())
    )
    telemetry = result.scalars().all()

    return [TelemetryOut.model_validate(t) for t in telemetry]
