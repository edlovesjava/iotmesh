from pydantic_settings import BaseSettings


class Settings(BaseSettings):
    database_url: str = "postgresql+asyncpg://mesh:meshpass123@localhost:5432/meshmanager"
    api_key: str = "dev-api-key"
    offline_threshold_seconds: int = 120  # Mark offline after 2 minutes

    class Config:
        env_file = ".env"


settings = Settings()
