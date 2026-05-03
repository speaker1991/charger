import os
from dataclasses import dataclass


@dataclass
class BotSettings:
	db_host: str = os.getenv("DB_HOST", "db")
	db_port: int = int(os.getenv("DB_PORT", "5432"))
	db_name: str = os.getenv("DB_NAME", "charging")
	db_user: str = os.getenv("DB_USER", "postgres")
	db_password: str = os.getenv("DB_PASSWORD", "postgres")
	bot_token: str = os.getenv("BOT_TOKEN", "")
	allowed_usernames_raw: str = os.getenv("BOT_ALLOWED_USERNAMES", "")
	api_url: str = os.getenv("API_URL", "http://api:8000")
	api_key: str = os.getenv("API_KEY", "")
	mqtt_host: str = os.getenv("MQTT_HOST", "mqtt")
	mqtt_port: int = int(os.getenv("MQTT_PORT", "1883"))

	@property
	def allowed_usernames(self) -> set[str]:
		items = [u.strip().lstrip("@").lower() for u in self.allowed_usernames_raw.split(",") if u.strip()]
		return set(items)

	@property
	def database_dsn(self) -> str:
		return (
			f"postgresql://{self.db_user}:{self.db_password}@{self.db_host}:{self.db_port}/{self.db_name}"
		)


settings = BotSettings()
