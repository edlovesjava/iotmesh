from .nodes import router as nodes_router
from .telemetry import router as telemetry_router
from .state import router as state_router
from .firmware import router as firmware_router
from .ota import router as ota_router

__all__ = ["nodes_router", "telemetry_router", "state_router", "firmware_router", "ota_router"]
