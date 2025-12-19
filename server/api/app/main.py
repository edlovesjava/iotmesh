import asyncio
from contextlib import asynccontextmanager
from datetime import datetime, timedelta
from pathlib import Path

from fastapi import FastAPI, Request
from fastapi.responses import HTMLResponse
from fastapi.templating import Jinja2Templates
from sqlalchemy import select, update

from .config import settings
from .database import async_session
from .models import Node
from .routers import nodes_router, telemetry_router, state_router, firmware_router, ota_router


# Background task for offline detection
async def check_offline_nodes():
    """Mark nodes as offline if no telemetry received recently."""
    while True:
        await asyncio.sleep(30)  # Check every 30 seconds
        try:
            async with async_session() as db:
                threshold = datetime.utcnow() - timedelta(seconds=settings.offline_threshold_seconds)
                await db.execute(
                    update(Node)
                    .where(Node.last_seen < threshold, Node.is_online == True)
                    .values(is_online=False)
                )
                await db.commit()
        except Exception as e:
            print(f"[OFFLINE CHECK] Error: {e}")


@asynccontextmanager
async def lifespan(app: FastAPI):
    # Startup: start background task
    task = asyncio.create_task(check_offline_nodes())
    yield
    # Shutdown: cancel background task
    task.cancel()
    try:
        await task
    except asyncio.CancelledError:
        pass


app = FastAPI(
    title="Mesh Manager API",
    description="Telemetry collection and management API for IoTMesh nodes",
    version="1.0.0",
    lifespan=lifespan,
)

# Templates for test UI
templates_dir = Path(__file__).parent / "templates"
templates = Jinja2Templates(directory=str(templates_dir))

# Include routers
app.include_router(nodes_router)
app.include_router(telemetry_router)
app.include_router(state_router)
app.include_router(firmware_router)
app.include_router(ota_router)


@app.get("/health")
async def health_check():
    """Health check endpoint."""
    return {"status": "healthy", "timestamp": datetime.utcnow().isoformat()}


@app.get("/test", response_class=HTMLResponse)
async def test_ui(request: Request):
    """Serve the test UI page."""
    return templates.TemplateResponse("test.html", {"request": request})


@app.get("/")
async def root():
    """Root endpoint with API info."""
    return {
        "name": "Mesh Manager API",
        "version": "1.0.0",
        "docs": "/docs",
        "test_ui": "/test",
        "health": "/health",
    }
