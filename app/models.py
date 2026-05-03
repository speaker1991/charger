from sqlalchemy import Column, Integer, Float, DateTime, func, String, Text, ForeignKey, Numeric
from sqlalchemy.orm import relationship, backref

from .database import Base


class Measurement(Base):
	__tablename__ = "measurements"

	id = Column(Integer, primary_key=True, index=True)
	current = Column(Float, nullable=False)
	voltage = Column(Float, nullable=False)
	station_id = Column(Integer, ForeignKey("stations.id"), nullable=False)
	created_at = Column(DateTime(timezone=True), server_default=func.now(), nullable=False)
	updated_at = Column(
		DateTime(timezone=True), server_default=func.now(), onupdate=func.now(), nullable=False
	)


class Address(Base):
	__tablename__ = "addresses"

	id = Column(Integer, primary_key=True, index=True)
	country_code = Column(String(2), nullable=False)  # ISO 3166-1 alpha-2
	postal_code = Column(String(20), nullable=True)
	region = Column(String(100), nullable=True)
	district = Column(String(100), nullable=True)
	city = Column(String(100), nullable=False)
	street = Column(String(200), nullable=True)
	house_number = Column(String(20), nullable=True)
	building = Column(String(20), nullable=True)  # корпус/строение
	apartment = Column(String(20), nullable=True)  # квартира/офис
	full_address = Column(Text, nullable=True)

	stations = relationship("Station", back_populates="address")


class Station(Base):
	__tablename__ = "stations"

	id = Column(Integer, primary_key=True, index=True)
	name = Column(String(255), nullable=False, index=True)
	description = Column(Text, nullable=True)
	latitude = Column(Float, nullable=True)
	longitude = Column(Float, nullable=True)
	address_id = Column(Integer, ForeignKey("addresses.id"), nullable=False)
	# Для проверок ключа храним соль и хэш PBKDF2
	api_key_salt = Column(String(64), nullable=False)
	api_key_hash = Column(String(128), nullable=False, index=True)
	# Быстрый поиск станции по ключу: SHA-256 отпечаток (hex)
	api_key_fingerprint = Column(String(64), nullable=True, index=True)
	created_at = Column(DateTime(timezone=True), server_default=func.now(), nullable=False)
	updated_at = Column(
		DateTime(timezone=True), server_default=func.now(), onupdate=func.now(), nullable=False
	)

	address = relationship("Address", back_populates="stations")


class PaymentTransaction(Base):
	__tablename__ = "payment_transactions"

	id = Column(Integer, primary_key=True, index=True)
	station_id = Column(Integer, ForeignKey("stations.id"), nullable=False)
	occurred_at = Column(DateTime(timezone=True), server_default=func.now(), nullable=False)
	amount_paid = Column(Numeric(12, 2), nullable=False)
	refund_amount = Column(Numeric(12, 2), nullable=False, server_default="0")
	kwh_spent = Column(Float, nullable=False)
	pistol = Column(Text, nullable=True)
	created_at = Column(DateTime(timezone=True), server_default=func.now(), nullable=False)
	updated_at = Column(
		DateTime(timezone=True), server_default=func.now(), onupdate=func.now(), nullable=False
	)


class EspDebugLog(Base):
	__tablename__ = "esp_debug_logs"

	id = Column(Integer, primary_key=True, index=True)
	station_id = Column(Integer, ForeignKey("stations.id"), nullable=False)
	occurred_at = Column(DateTime(timezone=True), server_default=func.now(), nullable=False)
	text1 = Column(Text, nullable=True)
	text2 = Column(Text, nullable=True)
	text3 = Column(Text, nullable=True)
	text4 = Column(Text, nullable=True)
	text5 = Column(Text, nullable=True)
	text6 = Column(Text, nullable=True)
	created_at = Column(DateTime(timezone=True), server_default=func.now(), nullable=False)


class StationCommand(Base):
	"""Команды отправленные на станции через MQTT."""
	__tablename__ = "station_commands"

	id = Column(Integer, primary_key=True, index=True)
	command_id = Column(String(36), nullable=False, unique=True, index=True)  # UUID
	station_id = Column(Integer, ForeignKey("stations.id"), nullable=False)
	command_type = Column(String(50), nullable=False, index=True)
	payload = Column(Text, nullable=True)  # JSON payload
	status = Column(String(20), nullable=False, default="pending", index=True)  # pending, sent, delivered, executed, failed
	sent_by = Column(String(100), nullable=True)  # username или 'api'
	response = Column(Text, nullable=True)  # JSON ответ от ESP32
	created_at = Column(DateTime(timezone=True), server_default=func.now(), nullable=False)
	sent_at = Column(DateTime(timezone=True), nullable=True)
	responded_at = Column(DateTime(timezone=True), nullable=True)

	station = relationship("Station")


class User(Base):
	"""Пользователь системы (super_admin, owner, employee)."""
	__tablename__ = "users"

	id = Column(Integer, primary_key=True, index=True)
	email = Column(String(255), nullable=False, unique=True, index=True)
	password_hash = Column(String(255), nullable=False)
	full_name = Column(String(255), nullable=False)
	phone = Column(String(50), nullable=True)
	role = Column(String(20), nullable=False, default="employee", index=True)  # super_admin, owner, employee
	owner_id = Column(Integer, ForeignKey("users.id"), nullable=True)  # for employees -> their owner
	is_active = Column(Integer, nullable=False, default=1)  # 1=active, 0=blocked
	created_at = Column(DateTime(timezone=True), server_default=func.now(), nullable=False)
	updated_at = Column(
		DateTime(timezone=True), server_default=func.now(), onupdate=func.now(), nullable=False
	)

	owner = relationship("User", remote_side=[id], backref="employees")
	station_access = relationship("UserStationAccess", back_populates="user", cascade="all, delete-orphan")


class Invite(Base):
	"""Инвайт-ссылка для регистрации."""
	__tablename__ = "invites"

	id = Column(Integer, primary_key=True, index=True)
	token = Column(String(64), nullable=False, unique=True, index=True)
	target_role = Column(String(20), nullable=False)  # owner, employee
	station_ids = Column(Text, nullable=True)  # JSON array of station IDs
	created_by = Column(Integer, ForeignKey("users.id"), nullable=False)
	is_used = Column(Integer, nullable=False, default=0)  # 0=unused, 1=used
	used_by = Column(Integer, ForeignKey("users.id"), nullable=True)
	created_at = Column(DateTime(timezone=True), server_default=func.now(), nullable=False)

	creator = relationship("User", foreign_keys=[created_by])


class UserStationAccess(Base):
	"""Маппинг пользователей к станциям."""
	__tablename__ = "user_station_access"

	id = Column(Integer, primary_key=True, index=True)
	user_id = Column(Integer, ForeignKey("users.id"), nullable=False)
	station_id = Column(Integer, ForeignKey("stations.id"), nullable=False)

	user = relationship("User", back_populates="station_access")
	station = relationship("Station")


class FreeChargingSession(Base):
	"""Сессия бесплатной зарядки сотрудника."""
	__tablename__ = "free_charging_sessions"

	id = Column(Integer, primary_key=True, index=True)
	user_id = Column(Integer, ForeignKey("users.id"), nullable=False)
	station_id = Column(Integer, ForeignKey("stations.id"), nullable=False)
	started_at = Column(DateTime(timezone=True), server_default=func.now(), nullable=False)
	kwh_spent = Column(Float, nullable=False, default=0.0)
	is_active = Column(Integer, nullable=False, default=1)  # 1=active, 0=finished
	finished_at = Column(DateTime(timezone=True), nullable=True)

	user = relationship("User")
	station = relationship("Station")