from datetime import datetime
from pydantic import BaseModel


class OTAUpdateCreate(BaseModel):
    """Request to create an OTA update job."""
    firmware_id: int
    target_node_id: str | None = None  # NULL = all nodes of type
    target_node_type: str | None = None  # Defaults to firmware's node_type
    force_update: bool = False


class OTAUpdateOut(BaseModel):
    """OTA update job response."""
    id: int
    firmware_id: int
    target_node_id: str | None = None
    target_node_type: str | None = None
    status: str
    force_update: bool
    created_at: datetime
    started_at: datetime | None = None
    completed_at: datetime | None = None

    class Config:
        from_attributes = True


class OTANodeStatusOut(BaseModel):
    """Per-node OTA progress."""
    node_id: str
    status: str
    current_part: int
    total_parts: int | None = None
    error_message: str | None = None
    started_at: datetime | None = None
    completed_at: datetime | None = None

    class Config:
        from_attributes = True


class OTAUpdateStatus(BaseModel):
    """Detailed OTA update status with node progress."""
    id: int
    firmware_id: int
    node_type: str
    version: str
    status: str
    force_update: bool
    created_at: datetime
    started_at: datetime | None = None
    completed_at: datetime | None = None
    nodes: list[OTANodeStatusOut] = []


class OTAPendingUpdate(BaseModel):
    """Pending update for gateway polling."""
    update_id: int
    firmware_id: int
    node_type: str
    version: str
    hardware: str
    md5: str
    num_parts: int
    size_bytes: int
    target_node_id: str | None = None
    force: bool


class OTAProgressReport(BaseModel):
    """Progress report from gateway."""
    current_part: int
    total_parts: int
    status: str = "downloading"
    error_message: str | None = None
