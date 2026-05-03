from typing import List
from datetime import datetime
from contextlib import asynccontextmanager
import json
import logging
import sys

from fastapi import FastAPI, Depends, HTTPException, Query, Security
from fastapi.middleware.cors import CORSMiddleware

# Настройка логирования
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s',
    stream=sys.stdout
)
from sqlalchemy import text
from sqlalchemy.orm import Session

from .database import get_db
from . import crud, schemas, models
from .security import (
    require_api_key, require_jwt_or_api_key,
    get_current_user, require_role,
    create_access_token, verify_password,
)
from .mqtt_service import mqtt_service, CommandType, get_mqtt_service, MQTTService
from .mqtt_ingest import mqtt_ingest


logger = logging.getLogger(__name__)


import asyncio
import random

FAKE_STATIONS = [
    {'name': 'ЭЗС №1', 'city': 'Москва', 'full_address': 'г. Москва, ул. Тверская, д.12'},
    {'name': 'ЭЗС №2', 'city': 'Санкт-Петербург', 'full_address': 'г. Санкт-Петербург, Невский пр., д.28'},
    {'name': 'ЭЗС №3', 'city': 'Казань', 'full_address': 'г. Казань, ул. Баумана, д.5'},
]

async def station_simulator():
    """Seed fake demo stations and publish simulated MQTT telemetry for them."""
    fake_ids = []
    try:
        from .database import SessionLocal
        from .models import Station, Address
        from .security import generate_station_api_key, station_key_fingerprint_hex

        db = SessionLocal()
        for fs in FAKE_STATIONS:
            existing = db.query(Station).filter(Station.name == fs['name']).first()
            if not existing:
                # Create address first
                addr = Address(
                    country_code='RU',
                    city=fs['city'],
                    full_address=fs['full_address'],
                )
                db.add(addr)
                db.flush()

                # Create station with proper api key
                plaintext_key, salt_hex, hash_hex = generate_station_api_key()
                fingerprint_hex = station_key_fingerprint_hex(plaintext_key)
                new_st = Station(
                    name=fs['name'],
                    description=f"Демо-станция {fs['name']}",
                    address_id=addr.id,
                    api_key_salt=salt_hex,
                    api_key_hash=hash_hex,
                    api_key_fingerprint=fingerprint_hex,
                )
                db.add(new_st)
                db.flush()
                fake_ids.append(new_st.id)
                logger.info(f"Seeded fake station: {fs['name']} (id={new_st.id})")
            else:
                fake_ids.append(existing.id)
        db.commit()
        db.close()
        logger.info(f"Fake station IDs for simulation: {fake_ids}")

        # --- Telemetry loop: only for fake stations ---
        while True:
            if mqtt_service._client and mqtt_service._client.is_connected:
                for sid in fake_ids:
                    charging = random.random() > 0.3  # 70% chance charging
                    payload = {
                        "device_id": str(sid),
                        "status": "online",
                        "voltage": round(random.uniform(218.0, 232.0), 1),
                        "temp_internal": round(random.uniform(32.0, 48.0), 1),
                        "current_power": round(random.uniform(8.0, 55.0), 2) if charging else 0.0,
                        "energy_total": round(random.uniform(800.0, 3500.0), 1),
                        "charging_active": charging,
                        "progress": random.randint(10, 95) if charging else 0,
                        "eta_minutes": random.randint(5, 60) if charging else 0,
                        "meter_error": False,
                    }
                    mqtt_service._client.publish(
                        f"ezs/{sid}/status", json.dumps(payload), qos=1
                    )
            await asyncio.sleep(5)
    except Exception as e:
        logger.error(f"Simulator error: {e}", exc_info=True)

@asynccontextmanager
async def lifespan(app: FastAPI):
    """Lifecycle hooks для FastAPI - запуск/остановка MQTT сервисов."""
    # Startup: запускаем MQTT сервисы
    logger.info("Starting MQTT services...")
    await mqtt_service.start()   # Для отправки команд
    await mqtt_ingest.start()    # Для приёма debug и payments
    # Start simulator
    asyncio.create_task(station_simulator())
    yield
    # Shutdown: останавливаем MQTT сервисы
    logger.info("Stopping MQTT services...")
    await mqtt_ingest.stop()
    await mqtt_service.stop()

app = FastAPI(title="Charging Station API", lifespan=lifespan)

# CORS for frontend
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=False,
    allow_methods=["*"],
    allow_headers=["*"],
)


@app.get("/")
def root():
	return {"status": "ok", "service": "Charging Station API"}


@app.get("/healthz")
def healthz(
	db: Session = Depends(get_db),
	api_key: str = Security(require_api_key),
):
	# Check DB connectivity
	db.execute(text("SELECT 1"))
	return {"status": "ok"}


# ================================================================
# AUTH ENDPOINTS
# ================================================================

@app.post("/auth/login", response_model=schemas.LoginResponse)
def auth_login(payload: schemas.LoginRequest, db: Session = Depends(get_db)):
	"""Авторизация по email/паролю. Возвращает JWT токен."""
	user = crud.get_user_by_email(db, payload.email)
	if not user or not verify_password(payload.password, user.password_hash):
		raise HTTPException(status_code=401, detail="Неверный email или пароль")
	if not user.is_active:
		raise HTTPException(status_code=403, detail="Аккаунт заблокирован")

	token = create_access_token({"sub": str(user.id), "role": user.role})
	station_ids = crud.get_user_station_ids(db, user.id)
	return schemas.LoginResponse(
		access_token=token,
		user=schemas.UserRead(
			id=user.id,
			email=user.email,
			full_name=user.full_name,
			phone=user.phone,
			role=user.role,
			owner_id=user.owner_id,
			is_active=user.is_active,
			created_at=user.created_at,
			station_ids=station_ids,
		),
	)


@app.get("/auth/me", response_model=schemas.UserRead)
def auth_me(
	user: models.User = Depends(get_current_user),
	db: Session = Depends(get_db),
):
	"""Получить данные текущего пользователя."""
	station_ids = crud.get_user_station_ids(db, user.id)
	return schemas.UserRead(
		id=user.id,
		email=user.email,
		full_name=user.full_name,
		phone=user.phone,
		role=user.role,
		owner_id=user.owner_id,
		is_active=user.is_active,
		created_at=user.created_at,
		station_ids=station_ids,
	)


@app.post("/auth/register", response_model=schemas.LoginResponse)
def auth_register(payload: schemas.RegisterRequest, db: Session = Depends(get_db)):
	"""Регистрация по инвайт-токену. Публичная регистрация запрещена."""
	# Validate invite
	invite = crud.get_invite_by_token(db, payload.invite_token)
	if not invite:
		raise HTTPException(status_code=400, detail="Недействительная ссылка приглашения")
	if invite.is_used:
		raise HTTPException(status_code=400, detail="Ссылка уже использована")

	# Check email uniqueness
	existing = crud.get_user_by_email(db, payload.email)
	if existing:
		raise HTTPException(status_code=400, detail="Пользователь с таким email уже существует")

	# Determine owner_id and station_ids from invite
	owner_id = None
	station_ids = None

	if invite.target_role == "employee":
		# Employee inherits owner from the invite creator
		creator = crud.get_user_by_id(db, invite.created_by)
		if creator and creator.role == "owner":
			owner_id = creator.id
		elif creator and creator.role == "super_admin":
			owner_id = creator.id

	if invite.station_ids:
		try:
			station_ids = json.loads(invite.station_ids)
		except json.JSONDecodeError:
			station_ids = []

	# Create user
	user = crud.create_user(
		db,
		email=payload.email,
		password=payload.password,
		full_name=payload.full_name,
		phone=payload.phone,
		role=invite.target_role,
		owner_id=owner_id,
		station_ids=station_ids,
	)

	# Mark invite as used
	crud.mark_invite_used(db, invite, user.id)

	# Generate token
	token = create_access_token({"sub": str(user.id), "role": user.role})
	user_station_ids = crud.get_user_station_ids(db, user.id)

	return schemas.LoginResponse(
		access_token=token,
		user=schemas.UserRead(
			id=user.id,
			email=user.email,
			full_name=user.full_name,
			phone=user.phone,
			role=user.role,
			owner_id=user.owner_id,
			is_active=user.is_active,
			created_at=user.created_at,
			station_ids=user_station_ids,
		),
	)


@app.get("/auth/validate-invite", response_model=schemas.InviteValidateResponse)
def validate_invite(token: str = Query(...), db: Session = Depends(get_db)):
	"""Проверить валидность инвайт-токена (для формы регистрации)."""
	invite = crud.get_invite_by_token(db, token)
	if not invite or invite.is_used:
		return schemas.InviteValidateResponse(valid=False)

	station_ids = None
	if invite.station_ids:
		try:
			station_ids = json.loads(invite.station_ids)
		except json.JSONDecodeError:
			station_ids = []

	return schemas.InviteValidateResponse(
		valid=True,
		target_role=invite.target_role,
		station_ids=station_ids,
	)


# ================================================================
# INVITE ENDPOINTS
# ================================================================

@app.post("/invites", response_model=schemas.InviteRead, status_code=201)
def create_invite(
	payload: schemas.InviteCreateRequest,
	user: models.User = Depends(get_current_user),
	db: Session = Depends(get_db),
):
	"""Создать инвайт-ссылку."""
	# RBAC: super_admin can create owners, owner can create employees
	if user.role == "super_admin":
		if payload.target_role not in ("owner", "employee"):
			raise HTTPException(status_code=400, detail="super_admin может создавать только owner или employee")
	elif user.role == "owner":
		if payload.target_role != "employee":
			raise HTTPException(status_code=403, detail="Owner может создавать только employee")
		# Validate that owner has access to the specified stations
		if payload.station_ids:
			owner_stations = crud.get_user_station_ids(db, user.id)
			for sid in payload.station_ids:
				if sid not in owner_stations:
					raise HTTPException(status_code=403, detail=f"У вас нет доступа к станции {sid}")
	else:
		raise HTTPException(status_code=403, detail="Недостаточно прав для создания приглашений")

	invite = crud.create_invite(
		db,
		target_role=payload.target_role,
		created_by=user.id,
		station_ids=payload.station_ids,
	)
	return invite


@app.get("/invites", response_model=List[schemas.InviteRead])
def list_invites(
	user: models.User = Depends(get_current_user),
	db: Session = Depends(get_db),
):
	"""Получить список инвайтов."""
	if user.role == "super_admin":
		return crud.list_invites(db)
	elif user.role == "owner":
		return crud.list_invites(db, created_by=user.id)
	raise HTTPException(status_code=403, detail="Недостаточно прав")


# ================================================================
# USER MANAGEMENT ENDPOINTS
# ================================================================

@app.get("/users", response_model=List[schemas.UserRead])
def list_users(
	user: models.User = Depends(get_current_user),
	db: Session = Depends(get_db),
):
	"""Получить список пользователей (с учётом RBAC)."""
	if user.role == "super_admin":
		users = crud.list_users(db)
	elif user.role == "owner":
		users = crud.list_users(db, owner_id=user.id)
	else:
		raise HTTPException(status_code=403, detail="Недостаточно прав")

	result = []
	for u in users:
		station_ids = crud.get_user_station_ids(db, u.id)
		result.append(schemas.UserRead(
			id=u.id,
			email=u.email,
			full_name=u.full_name,
			phone=u.phone,
			role=u.role,
			owner_id=u.owner_id,
			is_active=u.is_active,
			created_at=u.created_at,
			station_ids=station_ids,
		))
	return result


@app.patch("/users/{user_id}/toggle-active", response_model=schemas.UserRead)
def toggle_user_active(
	user_id: int,
	payload: schemas.UserToggleActive,
	user: models.User = Depends(get_current_user),
	db: Session = Depends(get_db),
):
	"""Заблокировать/разблокировать пользователя."""
	target = crud.get_user_by_id(db, user_id)
	if not target:
		raise HTTPException(status_code=404, detail="Пользователь не найден")

	# RBAC checks
	if user.role == "super_admin":
		pass  # can block anyone
	elif user.role == "owner":
		if target.owner_id != user.id:
			raise HTTPException(status_code=403, detail="Можно управлять только своими сотрудниками")
	else:
		raise HTTPException(status_code=403, detail="Недостаточно прав")

	updated = crud.toggle_user_active(db, user_id, payload.is_active)
	station_ids = crud.get_user_station_ids(db, updated.id)
	return schemas.UserRead(
		id=updated.id,
		email=updated.email,
		full_name=updated.full_name,
		phone=updated.phone,
		role=updated.role,
		owner_id=updated.owner_id,
		is_active=updated.is_active,
		created_at=updated.created_at,
		station_ids=station_ids,
	)


@app.delete("/users/{user_id}", status_code=204)
def delete_user(
	user_id: int,
	user: models.User = Depends(get_current_user),
	db: Session = Depends(get_db),
):
	"""Удалить пользователя (для Super Admin)."""
	if user.role != "super_admin":
		raise HTTPException(status_code=403, detail="Только Super Admin может удалять пользователей")
	
	target = crud.get_user_by_id(db, user_id)
	if not target:
		raise HTTPException(status_code=404, detail="Пользователь не найден")
		
	# Cannot delete yourself
	if target.id == user.id:
		raise HTTPException(status_code=400, detail="Нельзя удалить самого себя")
		
	# Убираем внешние ключи
	db.query(models.UserStationAccess).filter(models.UserStationAccess.user_id == target.id).delete()
	db.query(models.Invite).filter(models.Invite.created_by == target.id).delete()
	db.query(models.Invite).filter(models.Invite.used_by == target.id).update({models.Invite.used_by: None})
	db.query(models.User).filter(models.User.owner_id == target.id).update({models.User.owner_id: None})

	db.delete(target)
	db.commit()
	return None


# ================================================================
# FREE CHARGING ENDPOINTS
# ================================================================

@app.post("/free-charging/start", response_model=schemas.FreeChargingSessionRead, status_code=201)
async def start_free_charging_endpoint(
	payload: schemas.FreeChargingStart,
	user: models.User = Depends(get_current_user),
	db: Session = Depends(get_db),
	mqtt: MQTTService = Depends(get_mqtt_service),
):
	"""Начать бесплатную зарядку (запись в БД + отправка MQTT команды)."""
	# Check station access
	accessible = crud.get_accessible_station_ids(db, user)
	if accessible is not None and payload.station_id not in accessible:
		raise HTTPException(status_code=403, detail="Нет доступа к этой станции")

	# Publish to MQTT first to make sure it works
	# By default using Pistol A, if multiple needed we could pass in payload
	pistol = getattr(payload, "pistol", "A")
	
	try:
		success = await mqtt.publish_command(
			station_id=payload.station_id,
			command_type=f"free_charging_{pistol.lower()}", # or "free_charging"
			payload={"pistol": pistol},
			command_id="free_" + str(uuid.uuid4())[:8],
		)
		if not success:
			raise HTTPException(status_code=500, detail="Failed to send MQTT command")
	except Exception as e:
		logger.error(f"Error starting free charging: {e}")
		raise HTTPException(status_code=500, detail="Failed to communicate with station")

	session = crud.start_free_charging(db, user.id, payload.station_id)
	return session


@app.get("/free-charging/stats", response_model=List[schemas.EmployeeFreeChargingStats])
def get_free_charging_stats(
	date_from: datetime | None = Query(None),
	date_to: datetime | None = Query(None),
	user: models.User = Depends(get_current_user),
	db: Session = Depends(get_db),
):
	"""Получить статистику бесплатных зарядок сотрудников."""
	if user.role == "super_admin":
		return crud.get_employee_stats(db, date_from=date_from, date_to=date_to)
	elif user.role == "owner":
		return crud.get_employee_stats(db, date_from=date_from, date_to=date_to, owner_id=user.id)
	raise HTTPException(status_code=403, detail="Недостаточно прав")


# ================================================================
# STATIONS (legacy API key + JWT)
# ================================================================

@app.post("/stations", response_model=schemas.StationCreateResponse, status_code=201)
def create_station(
	payload: schemas.StationCreate,
	db: Session = Depends(get_db),
	auth: models.User | str = Depends(require_jwt_or_api_key),
):
	station, plaintext_key = crud.create_station(db, payload)
	return {
		"id": station.id,
		"name": station.name,
		"description": station.description,
		"latitude": station.latitude,
		"longitude": station.longitude,
		"address_id": station.address_id,
		"address": station.address,
		"created_at": station.created_at,
		"updated_at": station.updated_at,
		"api_key": plaintext_key,
	}


@app.get("/stations", response_model=List[schemas.StationReadWithAddress])
def list_stations(
	skip: int = Query(0, ge=0),
	limit: int = Query(100, gt=0, le=1000),
	db: Session = Depends(get_db),
	auth: models.User | str = Depends(require_jwt_or_api_key),
):
	stations = crud.list_stations(db, skip=skip, limit=limit)
	# If JWT user, filter by access
	if isinstance(auth, models.User) and auth.role != "super_admin":
		accessible = crud.get_accessible_station_ids(db, auth)
		if accessible is not None:
			stations = [s for s in stations if s.id in accessible]
	return stations


@app.get("/stations/{station_id}", response_model=schemas.StationReadWithAddress)
def get_station(
	station_id: int,
	db: Session = Depends(get_db),
	auth: models.User | str = Depends(require_jwt_or_api_key),
):
	station = crud.get_station(db, station_id)
	if not station:
		raise HTTPException(status_code=404, detail="Station not found")
	# Access check for JWT users
	if isinstance(auth, models.User) and auth.role != "super_admin":
		accessible = crud.get_accessible_station_ids(db, auth)
		if accessible is not None and station_id not in accessible:
			raise HTTPException(status_code=403, detail="Нет доступа к этой станции")
	return station


@app.post("/stations/{station_id}/rotate-key", response_model=schemas.StationRotateKeyResponse)
def rotate_station_key(
	station_id: int,
	db: Session = Depends(get_db),
	api_key: str = Security(require_api_key),
):
	result = crud.rotate_station_key(db, station_id)
	if not result:
		raise HTTPException(status_code=404, detail="Station not found")
	station, plaintext_key = result
	return {"id": station.id, "api_key": plaintext_key}


# ===== Addresses (админ) =====
@app.post("/addresses", response_model=schemas.AddressRead, status_code=201)
def create_address(
	payload: schemas.AddressCreate,
	db: Session = Depends(get_db),
	api_key: str = Security(require_api_key),
):
	return crud.create_address(db, payload)


@app.get("/addresses", response_model=List[schemas.AddressRead])
def list_addresses(
	skip: int = Query(0, ge=0),
	limit: int = Query(100, gt=0, le=1000),
	db: Session = Depends(get_db),
	api_key: str = Security(require_api_key),
):
	return crud.list_addresses(db, skip=skip, limit=limit)


@app.get("/addresses/{address_id}", response_model=schemas.AddressRead)
def get_address(
	address_id: int,
	db: Session = Depends(get_db),
	api_key: str = Security(require_api_key),
):
	address = crud.get_address(db, address_id)
	if not address:
		raise HTTPException(status_code=404, detail="Address not found")
	return address


@app.patch("/addresses/{address_id}", response_model=schemas.AddressRead)
def update_address(
	address_id: int,
	payload: schemas.AddressUpdate,
	db: Session = Depends(get_db),
	api_key: str = Security(require_api_key),
):
	address = crud.update_address(db, address_id, payload)
	if not address:
		raise HTTPException(status_code=404, detail="Address not found")
	return address


@app.delete("/addresses/{address_id}", status_code=204)
def delete_address(
	address_id: int,
	db: Session = Depends(get_db),
	api_key: str = Security(require_api_key),
):
	deleted = crud.delete_address(db, address_id)
	if not deleted:
		raise HTTPException(status_code=404, detail="Address not found")
	return None


# ===== Payments =====
# Примечание: Приём платежей теперь через MQTT (mqtt_ingest.py)

@app.get("/payments", response_model=List[schemas.PaymentTransactionRead])
def list_payments(
	skip: int = Query(0, ge=0),
	limit: int = Query(100, gt=0, le=1000),
	station_id: int | None = Query(None),
	date_from: datetime | None = Query(None),
	date_to: datetime | None = Query(None),
	db: Session = Depends(get_db),
	auth: models.User | str = Depends(require_jwt_or_api_key),
):
	return crud.list_payment_transactions(
		db, skip=skip, limit=limit, station_id=station_id, date_from=date_from, date_to=date_to
	)

# ===== Station Commands (MQTT) =====
# Примечание: Приём debug логов теперь через MQTT (mqtt_ingest.py)
@app.post("/stations/{station_id}/commands", response_model=schemas.StationCommandResponse, status_code=201)
async def send_command(
	station_id: int,
	payload: schemas.StationCommandCreate,
	db: Session = Depends(get_db),
	auth: models.User | str = Depends(require_jwt_or_api_key),
	mqtt: MQTTService = Depends(get_mqtt_service),
):
	"""
	Отправить команду на станцию через MQTT.

	Типы команд:
	- get_status: Запросить статус станции
	- stop_charging: Остановить зарядку
	- start_charging: Начать зарядку (payload: {pistol: str})
	- free_charging: Бесплатная зарядка (payload: {pistol: str})
	- restart: Перезагрузить ESP32
	- set_max_current: Установить макс. ток (payload: {max_current: float})
	- ping: Проверка связи
	"""
	# Проверяем что станция существует
	station = crud.get_station(db, station_id)
	if not station:
		raise HTTPException(status_code=404, detail="Station not found")

	# Валидируем тип команды
	valid_commands = ["get_status", "stop_charging", "start_charging", "free_charging", "restart", "set_max_current", "ping"]
	if payload.command_type not in valid_commands:
		raise HTTPException(
			status_code=400,
			detail=f"Invalid command_type. Must be one of: {', '.join(valid_commands)}"
		)

	# Создаём запись о команде в БД
	command = crud.create_command(db, station_id=station_id, data=payload)

	# Публикуем команду в MQTT
	try:
		command_type = CommandType(payload.command_type)
		success = await mqtt.publish_command(
			station_id=station_id,
			command_type=command_type,
			payload=payload.payload,
			command_id=command.command_id,
		)

		if success:
			crud.update_command_status(db, command.command_id, "sent")
			status = "sent"
			message = f"Command {payload.command_type} sent to station {station_id}"
		else:
			crud.update_command_status(db, command.command_id, "failed")
			status = "failed"
			message = "Failed to publish command to MQTT"

	except Exception as e:
		logger.error(f"Error sending command: {e}")
		crud.update_command_status(db, command.command_id, "failed")
		status = "failed"
		message = f"Error: {str(e)}"

	return schemas.StationCommandResponse(
		command_id=command.command_id,
		station_id=station_id,
		command_type=payload.command_type,
		status=status,
		message=message,
	)


@app.get("/stations/{station_id}/commands", response_model=List[schemas.StationCommandRead])
def list_station_commands(
	station_id: int,
	status: str | None = Query(None, description="Filter by status: pending, sent, delivered, executed, failed"),
	skip: int = Query(0, ge=0),
	limit: int = Query(100, gt=0, le=1000),
	db: Session = Depends(get_db),
	auth: models.User | str = Depends(require_jwt_or_api_key),
):
	"""Получить список команд для станции."""
	station = crud.get_station(db, station_id)
	if not station:
		raise HTTPException(status_code=404, detail="Station not found")

	return crud.list_commands(db, station_id=station_id, status=status, skip=skip, limit=limit)


@app.get("/commands/{command_id}", response_model=schemas.StationCommandRead)
def get_command(
	command_id: str,
	db: Session = Depends(get_db),
	auth: models.User | str = Depends(require_jwt_or_api_key),
):
	"""Получить информацию о команде по ID."""
	command = crud.get_command(db, command_id)
	if not command:
		raise HTTPException(status_code=404, detail="Command not found")
	return command


@app.get("/commands", response_model=List[schemas.StationCommandRead])
def list_all_commands(
	station_id: int | None = Query(None),
	status: str | None = Query(None),
	skip: int = Query(0, ge=0),
	limit: int = Query(100, gt=0, le=1000),
	db: Session = Depends(get_db),
	auth: models.User | str = Depends(require_jwt_or_api_key),
):
	"""Получить список всех команд с фильтрацией."""
	return crud.list_commands(db, station_id=station_id, status=status, skip=skip, limit=limit)


# ===== MQTT Health Check =====
@app.get("/mqtt/health")
async def mqtt_health(
	api_key: str = Security(require_api_key),
	mqtt: MQTTService = Depends(get_mqtt_service),
):
	"""Проверить подключение к MQTT брокеру."""
	is_healthy = await mqtt.health_check()
	if is_healthy:
		return {"status": "ok", "mqtt": "connected"}
	raise HTTPException(status_code=503, detail="MQTT broker unavailable")