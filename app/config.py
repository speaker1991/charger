from pydantic_settings import BaseSettings
from pydantic import Field


class Settings(BaseSettings):
	db_host: str = Field(default="db", alias="DB_HOST")
	db_port: int = Field(default=5432, alias="DB_PORT")
	db_name: str = Field(default="charging", alias="DB_NAME")
	db_user: str = Field(default="postgres", alias="DB_USER")
	db_password: str = Field(default="postgres", alias="DB_PASSWORD")
	api_key: str = Field(..., min_length=8, alias="API_KEY")

	# MQTT settings
	mqtt_host: str = Field(default="mqtt", alias="MQTT_HOST")
	mqtt_port: int = Field(default=1883, alias="MQTT_PORT")

	# JWT settings
	jwt_secret_key: str = Field(default="change-me-super-secret-jwt-key-2026", alias="JWT_SECRET_KEY")
	jwt_algorithm: str = Field(default="HS256", alias="JWT_ALGORITHM")
	jwt_expire_minutes: int = Field(default=1440, alias="JWT_EXPIRE_MINUTES")  # 24 hours

	@property
	def database_url(self) -> str:
		return (
			f"postgresql+psycopg2://{self.db_user}:{self.db_password}@{self.db_host}:{self.db_port}/{self.db_name}"
		)

	class Config:
		env_file = ".env"
		extra = "ignore"


settings = Settings()
