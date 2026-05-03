from datetime import datetime
from decimal import Decimal
from pydantic import BaseModel, Field


# ===== Stations =====

class StationBase(BaseModel):
	name: str = Field(..., min_length=1, max_length=255, description="Название станции")
	description: str | None = Field(None, description="Описание")
	latitude: float | None = Field(None, description="Широта")
	longitude: float | None = Field(None, description="Долгота")
	address_id: int = Field(..., description="ID адреса")


class StationCreate(StationBase):
	pass


class StationRead(StationBase):
	id: int
	created_at: datetime
	updated_at: datetime

	class Config:
		from_attributes = True


# ===== Addresses =====
class AddressBase(BaseModel):
	country_code: str = Field(..., min_length=2, max_length=2, description="ISO 3166-1 alpha-2")
	postal_code: str | None = Field(None, max_length=20)
	region: str | None = Field(None, max_length=100)
	district: str | None = Field(None, max_length=100)
	city: str = Field(..., max_length=100)
	street: str | None = Field(None, max_length=200)
	house_number: str | None = Field(None, max_length=20)
	building: str | None = Field(None, max_length=20)
	apartment: str | None = Field(None, max_length=20)
	full_address: str | None = Field(None, description="Полный адрес для поиска")


class AddressCreate(AddressBase):
	pass


class AddressRead(AddressBase):
	id: int

	class Config:
		from_attributes = True


class AddressUpdate(BaseModel):
	country_code: str | None = Field(None, min_length=2, max_length=2)
	postal_code: str | None = Field(None, max_length=20)
	region: str | None = Field(None, max_length=100)
	district: str | None = Field(None, max_length=100)
	city: str | None = Field(None, max_length=100)
	street: str | None = Field(None, max_length=200)
	house_number: str | None = Field(None, max_length=20)
	building: str | None = Field(None, max_length=20)
	apartment: str | None = Field(None, max_length=20)
	full_address: str | None = Field(None)


class StationReadWithAddress(StationRead):
	address: AddressRead | None = None


class StationCreateResponse(StationRead):
	# Возвращаем открытый ключ ТОЛЬКО при создании
	api_key: str = Field(..., description="API-ключ станции (показывается один раз)")
	address: AddressRead | None = None


# ===== Payments =====
class PaymentTransactionBase(BaseModel):
	occurred_at: datetime | None = Field(None, description="Время транзакции (если не указано — сейчас)")
	amount_paid: Decimal = Field(..., ge=0, description="Сумма оплаты, ₽")
	refund_amount: Decimal = Field(0, ge=0, description="Сумма возврата, ₽")
	kwh_spent: float = Field(..., ge=0, description="Потрачено кВт⋅ч")
	pistol: str | None = Field(None, description="Пистолет (идентификатор/название)")


class PaymentTransactionCreate(PaymentTransactionBase):
	pass


class PaymentTransactionRead(PaymentTransactionBase):
	id: int
	station_id: int
	occurred_at: datetime
	created_at: datetime
	updated_at: datetime

	class Config:
		from_attributes = True

class StationRotateKeyResponse(BaseModel):
	id: int
	api_key: str = Field(..., description="Новый API-ключ станции (показывается один раз)")


# ===== ESP Debug =====
class EspDebugBase(BaseModel):
	text1: str | None = Field(None, description="Поле 1")
	text2: str | None = Field(None, description="Поле 2")
	text3: str | None = Field(None, description="Поле 3")
	text4: str | None = Field(None, description="Поле 4")
	text5: str | None = Field(None, description="Поле 5")
	text6: str | None = Field(None, description="Поле 6")


class EspDebugCreate(EspDebugBase):
	occurred_at: datetime | None = Field(None, description="Время события (если не указано — сейчас)")


class EspDebugRead(EspDebugBase):
	id: int
	station_id: int
	occurred_at: datetime
	created_at: datetime

	class Config:
		from_attributes = True


# ===== Station Commands =====
class CommandType(str):
	"""Типы команд для ESP32."""
	GET_STATUS = "get_status"
	STOP_CHARGING = "stop_charging"
	START_CHARGING = "start_charging"
	RESTART = "restart"
	SET_MAX_CURRENT = "set_max_current"
	PING = "ping"


class StationCommandCreate(BaseModel):
	command_type: str = Field(..., description="Тип команды: get_status, stop_charging, start_charging, restart, set_max_current, ping")
	payload: dict | None = Field(None, description="Дополнительные параметры команды")
	sent_by: str | None = Field(None, description="Кто отправил команду (username)")


class StationCommandRead(BaseModel):
	id: int
	command_id: str
	station_id: int
	command_type: str
	payload: str | None
	status: str
	sent_by: str | None
	response: str | None
	created_at: datetime
	sent_at: datetime | None
	responded_at: datetime | None

	class Config:
		from_attributes = True


class StationCommandResponse(BaseModel):
	"""Ответ на отправку команды."""
	command_id: str
	station_id: int
	command_type: str
	status: str
	message: str


class CommandResponseUpdate(BaseModel):
	"""Обновление статуса команды от ESP32."""
	command_id: str
	status: str = Field(..., description="executed, failed")
	response: dict | None = Field(None, description="Данные ответа")


class StationStatusResponse(BaseModel):
	"""Статус станции (ответ от ESP32)."""
	station_id: int
	is_online: bool
	charging_active: bool = False
	current_power: float | None = None
	current_kwh: float | None = None
	pistol: str | None = None
	uptime_seconds: int | None = None
	last_seen: datetime | None = None


# ===== Auth Schemas =====
class LoginRequest(BaseModel):
	email: str = Field(..., description="Email (логин)")
	password: str = Field(..., min_length=4, description="Пароль")


class LoginResponse(BaseModel):
	access_token: str
	token_type: str = "bearer"
	user: "UserRead"


class RegisterRequest(BaseModel):
	email: str = Field(..., description="Email (логин)")
	password: str = Field(..., min_length=6, description="Пароль")
	full_name: str = Field(..., min_length=2, description="ФИО")
	phone: str | None = Field(None, description="Телефон")
	invite_token: str = Field(..., description="Инвайт-токен")


class UserRead(BaseModel):
	id: int
	email: str
	full_name: str
	phone: str | None
	role: str
	owner_id: int | None
	is_active: int
	created_at: datetime
	station_ids: list[int] = []

	class Config:
		from_attributes = True


class UserToggleActive(BaseModel):
	is_active: bool


# ===== Invite Schemas =====
class InviteCreateRequest(BaseModel):
	target_role: str = Field(..., description="Роль: owner или employee")
	station_ids: list[int] | None = Field(None, description="Список ID станций для доступа")


class InviteRead(BaseModel):
	id: int
	token: str
	target_role: str
	station_ids: str | None
	created_by: int
	is_used: int
	used_by: int | None
	created_at: datetime

	class Config:
		from_attributes = True


class InviteValidateResponse(BaseModel):
	valid: bool
	target_role: str | None = None
	station_ids: list[int] | None = None


# ===== Free Charging Schemas =====
class FreeChargingStart(BaseModel):
	station_id: int


class FreeChargingSessionRead(BaseModel):
	id: int
	user_id: int
	station_id: int
	started_at: datetime
	kwh_spent: float
	is_active: int
	finished_at: datetime | None

	class Config:
		from_attributes = True


class EmployeeFreeChargingStats(BaseModel):
	user_id: int
	full_name: str
	email: str
	session_count: int
	total_kwh: float