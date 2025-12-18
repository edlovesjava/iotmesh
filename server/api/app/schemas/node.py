from datetime import datetime
from pydantic import BaseModel


class NodeOut(BaseModel):
    id: str
    name: str | None = None
    firmware_version: str | None = None
    ip_address: str | None = None
    first_seen: datetime
    last_seen: datetime
    is_online: bool
    role: str
    peer_count: int | None = None

    class Config:
        from_attributes = True


class NodeUpdate(BaseModel):
    name: str
