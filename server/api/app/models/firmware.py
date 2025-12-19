from datetime import datetime
from sqlalchemy import String, Boolean, Integer, Text, LargeBinary, DateTime
from sqlalchemy.orm import Mapped, mapped_column

from ..database import Base


class Firmware(Base):
    __tablename__ = "firmware"

    id: Mapped[int] = mapped_column(Integer, primary_key=True)
    node_type: Mapped[str] = mapped_column(String(30), nullable=False)
    version: Mapped[str] = mapped_column(String(20), nullable=False)
    hardware: Mapped[str] = mapped_column(String(10), default="ESP32")
    filename: Mapped[str] = mapped_column(String(100), nullable=False)
    size_bytes: Mapped[int] = mapped_column(Integer, nullable=False)
    md5_hash: Mapped[str] = mapped_column(String(32), nullable=False)
    binary_data: Mapped[bytes] = mapped_column(LargeBinary, nullable=False)
    release_notes: Mapped[str | None] = mapped_column(Text, nullable=True)
    is_stable: Mapped[bool] = mapped_column(Boolean, default=False)
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=datetime.utcnow)
