from datetime import datetime
from pydantic import BaseModel


class TelemetryIn(BaseModel):
    uptime: int | None = None
    heap_free: int | None = None
    peer_count: int | None = None
    role: str | None = None
    firmware: str | None = None
    state: dict[str, str] | None = None


class TelemetryOut(BaseModel):
    time: datetime
    node_id: str
    heap_free: int | None = None
    uptime_sec: int | None = None
    peer_count: int | None = None
    role: str | None = None

    class Config:
        from_attributes = True


class StateOut(BaseModel):
    node_id: str
    key: str
    value: str | None = None
    version: int
    updated_at: datetime

    class Config:
        from_attributes = True
