from datetime import datetime
from sqlalchemy import String, Boolean, Integer, Text, DateTime, ForeignKey
from sqlalchemy.orm import Mapped, mapped_column, relationship

from ..database import Base


class OTAUpdate(Base):
    __tablename__ = "ota_updates"

    id: Mapped[int] = mapped_column(Integer, primary_key=True)
    firmware_id: Mapped[int] = mapped_column(Integer, ForeignKey("firmware.id", ondelete="CASCADE"))
    target_node_id: Mapped[str | None] = mapped_column(String(10), nullable=True)
    target_node_type: Mapped[str | None] = mapped_column(String(30), nullable=True)
    status: Mapped[str] = mapped_column(String(20), default="pending")
    force_update: Mapped[bool] = mapped_column(Boolean, default=False)
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=datetime.utcnow)
    started_at: Mapped[datetime | None] = mapped_column(DateTime(timezone=True), nullable=True)
    completed_at: Mapped[datetime | None] = mapped_column(DateTime(timezone=True), nullable=True)

    # Relationships
    firmware: Mapped["Firmware"] = relationship("Firmware")
    node_statuses: Mapped[list["OTANodeStatus"]] = relationship("OTANodeStatus", back_populates="update", cascade="all, delete-orphan")


class OTANodeStatus(Base):
    __tablename__ = "ota_node_status"

    id: Mapped[int] = mapped_column(Integer, primary_key=True)
    update_id: Mapped[int] = mapped_column(Integer, ForeignKey("ota_updates.id", ondelete="CASCADE"))
    node_id: Mapped[str] = mapped_column(String(10), nullable=False)
    status: Mapped[str] = mapped_column(String(20), default="pending")
    current_part: Mapped[int] = mapped_column(Integer, default=0)
    total_parts: Mapped[int | None] = mapped_column(Integer, nullable=True)
    error_message: Mapped[str | None] = mapped_column(Text, nullable=True)
    started_at: Mapped[datetime | None] = mapped_column(DateTime(timezone=True), nullable=True)
    completed_at: Mapped[datetime | None] = mapped_column(DateTime(timezone=True), nullable=True)

    # Relationships
    update: Mapped["OTAUpdate"] = relationship("OTAUpdate", back_populates="node_statuses")


# Import for type hints
from .firmware import Firmware
