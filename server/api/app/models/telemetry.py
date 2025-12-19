from datetime import datetime
from sqlalchemy import String, Integer, DateTime, Text
from sqlalchemy.orm import Mapped, mapped_column

from ..database import Base


class Telemetry(Base):
    __tablename__ = "telemetry"

    time: Mapped[datetime] = mapped_column(DateTime(timezone=True), primary_key=True)
    node_id: Mapped[str] = mapped_column(String(10), primary_key=True)
    heap_free: Mapped[int | None] = mapped_column(Integer, nullable=True)
    uptime_sec: Mapped[int | None] = mapped_column(Integer, nullable=True)
    peer_count: Mapped[int | None] = mapped_column(Integer, nullable=True)
    role: Mapped[str | None] = mapped_column(String(10), nullable=True)


class CurrentState(Base):
    __tablename__ = "current_state"

    node_id: Mapped[str] = mapped_column(String(10), primary_key=True)
    key: Mapped[str] = mapped_column(String(50), primary_key=True)
    value: Mapped[str | None] = mapped_column(Text, nullable=True)
    version: Mapped[int] = mapped_column(Integer, default=1)
    updated_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=datetime.utcnow)


class StateHistory(Base):
    __tablename__ = "state_history"

    time: Mapped[datetime] = mapped_column(DateTime(timezone=True), primary_key=True)
    node_id: Mapped[str] = mapped_column(String(10), primary_key=True)
    key: Mapped[str] = mapped_column(String(50), primary_key=True)
    value: Mapped[str | None] = mapped_column(Text, nullable=True)
    version: Mapped[int | None] = mapped_column(Integer, nullable=True)
