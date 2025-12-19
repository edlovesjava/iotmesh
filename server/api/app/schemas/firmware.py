from datetime import datetime
from pydantic import BaseModel


class FirmwareBase(BaseModel):
    node_type: str
    version: str
    hardware: str = "ESP32"
    release_notes: str | None = None
    is_stable: bool = False


class FirmwareCreate(FirmwareBase):
    """Used when uploading firmware via multipart form."""
    pass


class FirmwareOut(FirmwareBase):
    """Firmware metadata returned by API (excludes binary data)."""
    id: int
    filename: str
    size_bytes: int
    md5_hash: str
    created_at: datetime

    class Config:
        from_attributes = True


class FirmwareList(BaseModel):
    """List of firmware entries."""
    items: list[FirmwareOut]
    total: int
