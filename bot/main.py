import asyncio
import json
import uuid
from typing import Any
from contextlib import suppress
from datetime import datetime, timedelta, timezone
from decimal import Decimal
import os

import aiohttp
import asyncpg
import aiomqtt
from aiogram import Bot, Dispatcher
from aiogram.filters import Command
from aiogram.types import Message, CallbackQuery, InlineKeyboardMarkup, InlineKeyboardButton, ReplyKeyboardMarkup, KeyboardButton

from config import settings


CHANNEL_NAME = "payment_transactions_channel"
DEBUG_CHANNEL_NAME = "esp_debug_channel"


# ===== API Client =====
class APIClient:
	"""Клиент для взаимодействия с API сервером."""

	def __init__(self, base_url: str, api_key: str):
		self.base_url = base_url.rstrip("/")
		self.api_key = api_key
		self._session: aiohttp.ClientSession | None = None

	async def _get_session(self) -> aiohttp.ClientSession:
		if self._session is None or self._session.closed:
			self._session = aiohttp.ClientSession(
				headers={"X-API-Key": self.api_key}
			)
		return self._session

	async def close(self) -> None:
		if self._session and not self._session.closed:
			await self._session.close()

	async def get_stations(self) -> list[dict]:
		"""Получить список станций."""
		session = await self._get_session()
		try:
			async with session.get(f"{self.base_url}/stations") as resp:
				if resp.status == 200:
					return await resp.json()
				return []
		except Exception:
			return []

	async def get_station(self, station_id: int) -> dict | None:
		"""Получить информацию о станции с адресом."""
		session = await self._get_session()
		try:
			async with session.get(f"{self.base_url}/stations/{station_id}") as resp:
				if resp.status == 200:
					return await resp.json()
				return None
		except Exception:
			return None

	async def send_command(
		self,
		station_id: int,
		command_type: str,
		payload: dict | None = None,
		sent_by: str | None = None,
	) -> dict:
		"""Отправить команду на станцию."""
		session = await self._get_session()
		data = {
			"command_type": command_type,
			"payload": payload,
			"sent_by": sent_by,
		}
		try:
			async with session.post(
				f"{self.base_url}/stations/{station_id}/commands",
				json=data,
			) as resp:
				return await resp.json()
		except Exception as e:
			return {"status": "error", "message": str(e)}

	async def get_command_status(self, command_id: str) -> dict | None:
		"""Получить статус команды."""
		session = await self._get_session()
		try:
			async with session.get(f"{self.base_url}/commands/{command_id}") as resp:
				if resp.status == 200:
					data = await resp.json()
					print(f"[DEBUG] get_command_status({command_id}): status={data.get('status')}")
					return data
				else:
					print(f"[DEBUG] get_command_status({command_id}): HTTP {resp.status}")
				return None
		except Exception as e:
			print(f"[DEBUG] get_command_status({command_id}): Exception {e}")
			return None


api_client = APIClient(settings.api_url, settings.api_key)


# ===== MQTT Command Service =====
class MQTTCommandService:
	"""Сервис для отправки команд через MQTT и ожидания ответов."""

	def __init__(self, host: str, port: int):
		self.host = host
		self.port = port
		self._pending_commands: dict[str, asyncio.Future] = {}
		self._mqtt_task: asyncio.Task | None = None
		self._client: aiomqtt.Client | None = None
		self._connected = asyncio.Event()

	async def start(self) -> None:
		"""Запустить MQTT listener в фоне."""
		self._mqtt_task = asyncio.create_task(self._mqtt_listener())
		# Ждём подключения до 5 секунд
		try:
			await asyncio.wait_for(self._connected.wait(), timeout=5.0)
		except asyncio.TimeoutError:
			print("[MQTT] Warning: Connection timeout, will retry in background")

	async def stop(self) -> None:
		"""Остановить MQTT listener."""
		if self._mqtt_task:
			self._mqtt_task.cancel()
			with suppress(asyncio.CancelledError):
				await self._mqtt_task

	async def _mqtt_listener(self) -> None:
		"""Слушать ответы от станций."""
		while True:
			try:
				async with aiomqtt.Client(
					hostname=self.host,
					port=self.port,
					identifier=f"tg-bot-{uuid.uuid4().hex[:8]}",
				) as client:
					self._client = client
					# Подписываемся на ответы от всех станций
					await client.subscribe("stations/+/response")
					print(f"[MQTT] Connected to {self.host}:{self.port}, subscribed to stations/+/response")
					self._connected.set()

					async for message in client.messages:
						try:
							payload = json.loads(message.payload.decode())
							command_id = payload.get("command_id")
							print(f"[MQTT] Received message: topic={message.topic}, command_id={command_id}")
							print(f"[MQTT] Payload: {payload}")
							if command_id and command_id in self._pending_commands:
								future = self._pending_commands.pop(command_id)
								if not future.done():
									future.set_result(payload)
									print(f"[MQTT] Resolved future for command_id={command_id}")
							else:
								print(f"[MQTT] No pending command for command_id={command_id}")
						except Exception as e:
							print(f"[MQTT] Error processing message: {e}")

			except aiomqtt.MqttError as e:
				print(f"[MQTT] Connection error: {e}, reconnecting in 5s...")
				self._connected.clear()
				self._client = None
				await asyncio.sleep(5)
			except asyncio.CancelledError:
				break
			except Exception as e:
				print(f"[MQTT] Unexpected error: {e}, reconnecting in 5s...")
				self._connected.clear()
				self._client = None
				await asyncio.sleep(5)

	async def send_command(
		self,
		station_id: int,
		command_type: str,
		payload: dict | None = None,
		timeout: float = 10.0,
	) -> dict:
		"""
		Отправить команду на станцию и дождаться ответа.

		Returns:
			{"status": "executed/failed", "message": "...", ...} или
			{"status": "timeout", "message": "..."} при таймауте или
			{"status": "error", "message": "..."} при ошибке
		"""
		if not self._client or not self._connected.is_set():
			return {"status": "error", "message": "MQTT not connected"}

		command_id = str(uuid.uuid4())
		topic = f"stations/{station_id}/commands"

		message = {
			"command": command_type,
			"command_id": command_id,
			"timestamp": datetime.now(timezone.utc).isoformat(),
			"payload": payload or {},
		}

		# Создаём Future для ожидания ответа
		future: asyncio.Future = asyncio.get_event_loop().create_future()
		self._pending_commands[command_id] = future

		try:
			# Отправляем команду
			await self._client.publish(topic, json.dumps(message), qos=1)
			print(f"[MQTT] Sent command {command_type} to station {station_id}, id={command_id}")

			# Ждём ответ
			response = await asyncio.wait_for(future, timeout=timeout)
			return response

		except asyncio.TimeoutError:
			self._pending_commands.pop(command_id, None)
			return {"status": "timeout", "message": f"No response within {timeout}s"}
		except Exception as e:
			self._pending_commands.pop(command_id, None)
			return {"status": "error", "message": str(e)}


mqtt_service = MQTTCommandService(settings.mqtt_host, settings.mqtt_port)


# ===== Stats functions =====
async def aggregate_stats(
	conn: asyncpg.Connection,
	date_from: datetime,
	date_to: datetime,
) -> dict[str, Decimal | float]:
	row = await conn.fetchrow(
		"""
		SELECT
			COALESCE(SUM(amount_paid), 0) AS total_paid,
			COALESCE(SUM(refund_amount), 0) AS total_refund,
			COALESCE(SUM(amount_paid - refund_amount), 0) AS profit,
			COALESCE(SUM(kwh_spent), 0) AS total_kwh
		FROM payment_transactions
		WHERE occurred_at >= $1 AND occurred_at < $2
		""",
		date_from,
		date_to,
	)
	return {
		"total_paid": row["total_paid"] if row and row["total_paid"] is not None else Decimal("0"),
		"total_refund": row["total_refund"] if row and row["total_refund"] is not None else Decimal("0"),
		"profit": row["profit"] if row and row["profit"] is not None else Decimal("0"),
		"total_kwh": float(row["total_kwh"]) if row and row["total_kwh"] is not None else 0.0,
	}


def format_money(value: Decimal | float) -> str:
	try:
		return f"{Decimal(value):.2f}"
	except Exception:
		try:
			return f"{float(value):.2f}"
		except Exception:
			return str(value)


def format_kwh(value: float) -> str:
	try:
		return f"{float(value):.2f}"
	except Exception:
		return str(value)


# ===== Keyboard builders =====
def build_period_keyboard() -> InlineKeyboardMarkup:
	return InlineKeyboardMarkup(
		inline_keyboard=[
			[InlineKeyboardButton(text="Сегодня", callback_data="stats_today")],
			[InlineKeyboardButton(text="Неделя", callback_data="stats_week")],
			[InlineKeyboardButton(text="Месяц", callback_data="stats_month")],
			[InlineKeyboardButton(text="Произвольная дата", callback_data="stats_custom")],
		]
	)


def build_main_keyboard(is_debug_on: bool) -> ReplyKeyboardMarkup:
	toggle_text = "🔇 Отключить дебаг" if is_debug_on else "🔊 Включить дебаг"
	return ReplyKeyboardMarkup(
		keyboard=[
			[KeyboardButton(text="📊 Статистика"), KeyboardButton(text="🔌 Станции")],
			[KeyboardButton(text=toggle_text)],
		],
		resize_keyboard=True,
		one_time_keyboard=False,
	)


def build_stations_keyboard(stations: list[dict]) -> InlineKeyboardMarkup:
	"""Клавиатура для выбора станции."""
	buttons = []
	for station in stations:
		station_id = station.get("id")
		name = station.get("name", f"Станция {station_id}")
		# Показываем ID и название
		buttons.append([InlineKeyboardButton(
			text=f"🔌 #{station_id} — {name}",
			callback_data=f"station_{station_id}"
		)])
	buttons.append([InlineKeyboardButton(text="❌ Отмена", callback_data="cancel")])
	return InlineKeyboardMarkup(inline_keyboard=buttons)


def build_station_commands_keyboard(station_id: int, station_name: str = "") -> InlineKeyboardMarkup:
	"""Клавиатура команд для станции."""
	return InlineKeyboardMarkup(
		inline_keyboard=[
			[InlineKeyboardButton(text="ℹ️ Информация", callback_data=f"info_{station_id}")],
			[InlineKeyboardButton(text="📡 Статус", callback_data=f"cmd_{station_id}_get_status")],
			[InlineKeyboardButton(text="💳 Терминалы", callback_data=f"terminals_{station_id}")],
			[InlineKeyboardButton(text="🔄 Перезагрузить", callback_data=f"cmd_{station_id}_restart")],
			[InlineKeyboardButton(text="⬅️ Назад", callback_data="stations_list")],
		]
	)


def build_terminals_keyboard(
	station_id: int,
	terminal_a_enabled: bool = True,
	terminal_b_enabled: bool = True,
	free_charging_a: bool = False,
	free_charging_b: bool = False,
	meter_error_a: bool = False,
	meter_error_b: bool = False,
) -> InlineKeyboardMarkup:
	"""Клавиатура управления терминалами A и B с отображением текущего статуса."""
	
	buttons = []
	
	# Если есть ошибки счётчиков - не показываем кнопки управления для этих терминалов
	if meter_error_a and meter_error_b:
		# Оба счётчика недоступны - только кнопки обновления и назад
		pass
	else:
		# Кнопки терминалов (только для работающих)
		term_row = []
		if not meter_error_a:
			if terminal_a_enabled:
				term_row.append(InlineKeyboardButton(text="✅ Терминал A ВКЛ", callback_data=f"cmd_{station_id}_disable_terminal_a"))
			else:
				term_row.append(InlineKeyboardButton(text="🔴 Терминал A ВЫКЛ", callback_data=f"cmd_{station_id}_enable_terminal_a"))
		
		if not meter_error_b:
			if terminal_b_enabled:
				term_row.append(InlineKeyboardButton(text="✅ Терминал B ВКЛ", callback_data=f"cmd_{station_id}_disable_terminal_b"))
			else:
				term_row.append(InlineKeyboardButton(text="🔴 Терминал B ВЫКЛ", callback_data=f"cmd_{station_id}_enable_terminal_b"))
		
		if term_row:
			buttons.append(term_row)
		
		# Кнопки режима зарядки (только для работающих терминалов)
		charge_row = []
		if not meter_error_a:
			if free_charging_a:
				charge_row.append(InlineKeyboardButton(text="🆓 A: Бесплатная", callback_data=f"cmd_{station_id}_disable_free_charging_a"))
			else:
				charge_row.append(InlineKeyboardButton(text="💰 A: Платная", callback_data=f"cmd_{station_id}_free_charging_a"))
		
		if not meter_error_b:
			if free_charging_b:
				charge_row.append(InlineKeyboardButton(text="🆓 B: Бесплатная", callback_data=f"cmd_{station_id}_disable_free_charging_b"))
			else:
				charge_row.append(InlineKeyboardButton(text="💰 B: Платная", callback_data=f"cmd_{station_id}_free_charging_b"))
		
		if charge_row:
			buttons.append(charge_row)
	
	# Всегда показываем кнопки обновления и назад
	buttons.append([InlineKeyboardButton(text="🔄 Обновить", callback_data=f"terminals_{station_id}")])
	buttons.append([InlineKeyboardButton(text="⬅️ Назад к станции", callback_data=f"station_{station_id}")])
	
	return InlineKeyboardMarkup(inline_keyboard=buttons)


# Кэш названий станций
stations_cache: dict[int, str] = {}


async def get_station_name(station_id: int) -> str:
	"""Получить название станции из кэша или API."""
	if station_id in stations_cache:
		return stations_cache[station_id]
	
	stations = await api_client.get_stations()
	for station in stations:
		stations_cache[station.get("id")] = station.get("name", f"Станция {station.get('id')}")
	
	return stations_cache.get(station_id, f"Станция {station_id}")


def start_of_today_utc() -> datetime:
	now = datetime.now(timezone.utc)
	return now.replace(hour=0, minute=0, second=0, microsecond=0)


def last_n_days_utc(n: int) -> tuple[datetime, datetime]:
	now = datetime.now(timezone.utc)
	return now - timedelta(days=n), now


def format_stats_message(
	title: str,
	date_from: datetime,
	date_to: datetime,
	total_paid: Decimal | float,
	total_refund: Decimal | float,
	profit: Decimal | float,
	total_kwh: float,
) -> str:
	offset_hours = int(os.getenv("BOT_TZ_OFFSET_HOURS", "7"))
	local_tz = timezone(timedelta(hours=offset_hours))
	label = f"GMT{offset_hours:+d}"
	df_local = date_from.astimezone(local_tz)
	dt_local = date_to.astimezone(local_tz)
	return (
		f"📊 Статистика {title}\n\n"
		f"🕐 Период ({label}): {df_local:%Y-%m-%d %H:%M} — {dt_local:%Y-%m-%d %H:%M}\n\n"
		f"💵 Сумма оплат: {format_money(total_paid)} ₽\n"
		f"↩️ Сумма возвратов: {format_money(total_refund)} ₽\n"
		f"💰 Итог: {format_money(profit)} ₽\n\n"
		f"⚡️ Расход: {format_kwh(total_kwh)} кВт⋅ч"
	)


# ===== Subscriber functions =====
async def upsert_subscriber(conn: asyncpg.Connection, username: str, chat_id: int) -> None:
	await conn.execute(
		"""
		INSERT INTO bot_subscribers(username, chat_id)
		VALUES($1, $2)
		ON CONFLICT (username) DO UPDATE SET chat_id = EXCLUDED.chat_id;
		""",
		username,
		chat_id,
	)


async def remove_subscriber(conn: asyncpg.Connection, username: str) -> None:
	await conn.execute("DELETE FROM bot_subscribers WHERE username = $1", username)


async def get_all_chat_ids(conn: asyncpg.Connection) -> list[int]:
	rows = await conn.fetch("SELECT chat_id FROM bot_subscribers")
	return [r[0] for r in rows]


async def upsert_debug_subscriber(conn: asyncpg.Connection, username: str, chat_id: int) -> None:
	await conn.execute(
		"""
		INSERT INTO bot_debug_subscribers(username, chat_id)
		VALUES($1, $2)
		ON CONFLICT (username) DO UPDATE SET chat_id = EXCLUDED.chat_id;
		""",
		username,
		chat_id,
	)


async def remove_debug_subscriber(conn: asyncpg.Connection, username: str) -> None:
	await conn.execute("DELETE FROM bot_debug_subscribers WHERE username = $1", username)


async def get_all_debug_chat_ids(conn: asyncpg.Connection) -> list[int]:
	rows = await conn.fetch("SELECT chat_id FROM bot_debug_subscribers")
	return [r[0] for r in rows]


async def is_debug_subscribed(conn: asyncpg.Connection, username: str) -> bool:
	row = await conn.fetchrow("SELECT 1 FROM bot_debug_subscribers WHERE username = $1 LIMIT 1", username)
	return row is not None


# ===== Listeners =====
async def listen_and_broadcast(bot: Bot, dsn: str) -> None:
	print(f"[LISTEN] Connecting to PostgreSQL for {CHANNEL_NAME}...")
	try:
		conn = await asyncpg.connect(dsn)
		print(f"[LISTEN] Connected! Setting up listener for {CHANNEL_NAME}")
	except Exception as e:
		print(f"[LISTEN] ERROR: Failed to connect to PostgreSQL: {e}")
		return
	
	queue: asyncio.Queue[str] = asyncio.Queue()

	def listener(_connection: asyncpg.Connection, _pid: int, _channel: str, payload: str) -> None:
		print(f"[LISTEN] Received NOTIFY on {_channel}: {payload[:100]}...")
		queue.put_nowait(payload)

	await conn.add_listener(CHANNEL_NAME, listener)
	await conn.execute(f"LISTEN {CHANNEL_NAME};")
	print(f"[LISTEN] Now listening on {CHANNEL_NAME}")
	
	try:
		while True:
			payload_text = await queue.get()
			print(f"[LISTEN] Processing payment notification...")
			try:
				payload: dict[str, Any] = json.loads(payload_text)
			except Exception as e:
				print(f"[LISTEN] ERROR parsing JSON: {e}")
				continue
			station_id = payload.get("station_id")
			amount_paid = payload.get("amount_paid")
			refund_amount = payload.get("refund_amount")
			kwh_spent_raw = payload.get("kwh_spent")
			pistol = payload.get("pistol")
			try:
				kwh_spent_fmt = f"{float(kwh_spent_raw):.2f}"
			except (TypeError, ValueError):
				kwh_spent_fmt = str(kwh_spent_raw)
			text = (
				f"💳 Новая транзакция оплаты\n"
				f"🔌 Станция: {station_id}\n"
				f"🔫 Пистолет: {pistol}\n"
				f"💵 Оплата: {amount_paid} ₽\n"
				f"↩️ Возврат: {refund_amount} ₽\n"
				f"⚡️ Расход: {kwh_spent_fmt} кВт⋅ч"
			)
			chat_conn = await asyncpg.connect(dsn)
			try:
				chat_ids = await get_all_chat_ids(chat_conn)
				print(f"[LISTEN] Sending to {len(chat_ids)} subscribers")
				for chat_id in chat_ids:
					try:
						await bot.send_message(chat_id=chat_id, text=text)
						print(f"[LISTEN] Sent to chat_id={chat_id}")
					except Exception as e:
						print(f"[LISTEN] ERROR sending to {chat_id}: {e}")
			finally:
				await chat_conn.close()
	except asyncio.CancelledError:
		print(f"[LISTEN] Cancelled")
	except Exception as e:
		print(f"[LISTEN] ERROR in listener loop: {e}")
	finally:
		with suppress(Exception):
			await conn.remove_listener(CHANNEL_NAME, listener)
		with suppress(Exception):
			await conn.close()
		print(f"[LISTEN] Disconnected from {CHANNEL_NAME}")


async def listen_and_broadcast_debug(bot: Bot, dsn: str) -> None:
	print(f"[LISTEN] Connecting to PostgreSQL for {DEBUG_CHANNEL_NAME}...")
	try:
		conn = await asyncpg.connect(dsn)
		print(f"[LISTEN] Connected! Setting up listener for {DEBUG_CHANNEL_NAME}")
	except Exception as e:
		print(f"[LISTEN] ERROR: Failed to connect to PostgreSQL for debug: {e}")
		return
	
	queue: asyncio.Queue[str] = asyncio.Queue()

	def listener(_connection: asyncpg.Connection, _pid: int, _channel: str, payload: str) -> None:
		queue.put_nowait(payload)

	await conn.add_listener(DEBUG_CHANNEL_NAME, listener)
	await conn.execute(f"LISTEN {DEBUG_CHANNEL_NAME};")
	print(f"[LISTEN] Now listening on {DEBUG_CHANNEL_NAME}")
	
	try:
		while True:
			payload_text = await queue.get()
			try:
				payload: dict[str, Any] = json.loads(payload_text)
			except Exception:
				continue
			station_id = payload.get("station_id")
			text1 = payload.get("text1")
			text2 = payload.get("text2")
			text3 = payload.get("text3")
			text4 = payload.get("text4")
			text5 = payload.get("text5")
			text6 = payload.get("text6")
			lines = [
				"🔧 Новое отладочное сообщение",
				f"Станция: {station_id}",
			]
			for part in (text1, text2, text3, text4, text5, text6):
				if part:
					lines.append(str(part))
			text = "\n".join(lines)
			chat_conn = await asyncpg.connect(dsn)
			try:
				chat_ids = await get_all_debug_chat_ids(chat_conn)
				for chat_id in chat_ids:
					try:
						await bot.send_message(chat_id=chat_id, text=text)
					except Exception:
						pass
			finally:
				await chat_conn.close()
	except asyncio.CancelledError:
		pass
	finally:
		with suppress(Exception):
			await conn.remove_listener(DEBUG_CHANNEL_NAME, listener)
		with suppress(Exception):
			await conn.close()


def normalize_username(username: str | None) -> str:
	return (username or "").strip().lstrip("@").lower()


# ===== Command name mapping =====
COMMAND_NAMES = {
	"get_status": "Статус",
	"stop_charging": "Остановить зарядку",
	"start_charging": "Начать зарядку",
	"restart": "Перезагрузить",
	"set_max_current": "Установить макс. ток",
	"enable_terminal_a": "Включить терминал A",
	"disable_terminal_a": "Отключить терминал A",
	"enable_terminal_b": "Включить терминал B",
	"disable_terminal_b": "Отключить терминал B",
	"free_charging_a": "Бесплатная зарядка A",
	"free_charging_b": "Бесплатная зарядка B",
	"disable_free_charging_a": "Отключить бесплатную A",
	"disable_free_charging_b": "Отключить бесплатную B",
}

COMMAND_EMOJIS = {
	"get_status": "📡",
	"stop_charging": "🔴",
	"start_charging": "🟢",
	"restart": "🔄",
	"set_max_current": "⚡️",
	"enable_terminal_a": "🟢",
	"disable_terminal_a": "🔴",
	"enable_terminal_b": "🟢",
	"disable_terminal_b": "🔴",
	"free_charging_a": "🆓",
	"free_charging_b": "🆓",
	"disable_free_charging_a": "💰",
	"disable_free_charging_b": "💰",
}


async def main() -> None:
	pending_custom_input: set[int] = set()
	if not settings.bot_token:
		raise RuntimeError("BOT_TOKEN не задан. Установите переменную окружения.")

	bot = Bot(token=settings.bot_token)
	dp = Dispatcher()

	allowed = settings.allowed_usernames

	@dp.message(Command("start"))
	async def start_handler(message: Message) -> None:
		uname = normalize_username(message.from_user.username)
		if uname not in allowed:
			await message.answer("У вас нет доступа к боту.")
			return
		conn_l = await asyncpg.connect(settings.database_dsn)
		try:
			await upsert_subscriber(conn_l, uname, message.chat.id)
			subscribed = await is_debug_subscribed(conn_l, uname)
		finally:
			await conn_l.close()
		await message.answer(
			"👋 Добро пожаловать!\n\n"
			"Подписка оформлена. Вы будете получать уведомления о транзакциях.\n\n"
			"📊 Статистика — просмотр статистики\n"
			"🔌 Станции — управление станциями\n"
			"🔊/🔇 Дебаг — отладочные сообщения",
			reply_markup=build_main_keyboard(subscribed)
		)

	@dp.message(Command("stop"))
	async def stop_handler(message: Message) -> None:
		uname = normalize_username(message.from_user.username)
		conn_l = await asyncpg.connect(settings.database_dsn)
		try:
			await remove_subscriber(conn_l, uname)
		finally:
			await conn_l.close()
		await message.answer("Подписка отменена.")

	@dp.message(Command("debug_on"))
	async def debug_on_handler(message: Message) -> None:
		uname = normalize_username(message.from_user.username)
		if uname not in allowed:
			await message.answer("У вас нет доступа к боту.")
			return
		conn_l = await asyncpg.connect(settings.database_dsn)
		try:
			await upsert_debug_subscriber(conn_l, uname, message.chat.id)
		finally:
			await conn_l.close()
		await message.answer("Подписка на отладочные сообщения включена.", reply_markup=build_main_keyboard(True))

	@dp.message(Command("debug_off"))
	async def debug_off_handler(message: Message) -> None:
		uname = normalize_username(message.from_user.username)
		conn_l = await asyncpg.connect(settings.database_dsn)
		try:
			await remove_debug_subscriber(conn_l, uname)
		finally:
			await conn_l.close()
		await message.answer("Подписка на отладочные сообщения отключена.", reply_markup=build_main_keyboard(False))

	@dp.message(Command("stats"))
	async def stats_entry(message: Message) -> None:
		uname = normalize_username(message.from_user.username)
		if uname not in allowed:
			await message.answer("У вас нет доступа к боту.")
			return
		kb = build_period_keyboard()
		await message.answer("Выберите период:", reply_markup=kb)

	@dp.message(Command("stations"))
	async def stations_handler(message: Message) -> None:
		"""Показать список станций для управления."""
		uname = normalize_username(message.from_user.username)
		if uname not in allowed:
			await message.answer("У вас нет доступа к боту.")
			return

		stations = await api_client.get_stations()
		if not stations:
			await message.answer("❌ Станции не найдены или API недоступен.")
			return

		# Обновляем кэш названий
		for station in stations:
			stations_cache[station.get("id")] = station.get("name", f"Станция {station.get('id')}")

		kb = build_stations_keyboard(stations)
		await message.answer("🔌 Выберите станцию:", reply_markup=kb)

	# ===== Callback handlers =====
	@dp.callback_query(lambda c: c.data == "cancel")
	async def cancel_callback(query: CallbackQuery) -> None:
		if query.message:
			try:
				await query.message.delete()
			except Exception:
				pass
		await query.answer("Отменено")

	@dp.callback_query(lambda c: c.data == "stations_list")
	async def stations_list_callback(query: CallbackQuery) -> None:
		"""Вернуться к списку станций."""
		uname = normalize_username(query.from_user.username)
		if uname not in allowed:
			await query.answer("Нет доступа", show_alert=True)
			return

		stations = await api_client.get_stations()
		if not stations:
			await query.answer("Станции не найдены", show_alert=True)
			return

		# Обновляем кэш названий
		for station in stations:
			stations_cache[station.get("id")] = station.get("name", f"Станция {station.get('id')}")

		kb = build_stations_keyboard(stations)
		if query.message:
			try:
				await query.message.edit_text("🔌 Выберите станцию:", reply_markup=kb)
			except Exception:
				await query.message.answer("🔌 Выберите станцию:", reply_markup=kb)
		await query.answer()

	@dp.callback_query(lambda c: c.data and c.data.startswith("station_"))
	async def station_select_callback(query: CallbackQuery) -> None:
		"""Показать команды для выбранной станции."""
		uname = normalize_username(query.from_user.username)
		if uname not in allowed:
			await query.answer("Нет доступа", show_alert=True)
			return

		station_id = int(query.data.replace("station_", ""))
		station_name = await get_station_name(station_id)
		kb = build_station_commands_keyboard(station_id, station_name)

		if query.message:
			try:
				await query.message.edit_text(
					f"⚙️ Станция #{station_id} — {station_name}\n\nВыберите команду:",
					reply_markup=kb
				)
			except Exception:
				await query.message.answer(
					f"⚙️ Станция #{station_id} — {station_name}\n\nВыберите команду:",
					reply_markup=kb
				)
		await query.answer()

	@dp.callback_query(lambda c: c.data and c.data.startswith("info_"))
	async def station_info_callback(query: CallbackQuery) -> None:
		"""Показать информацию о станции (адрес, координаты и т.д.)."""
		uname = normalize_username(query.from_user.username)
		if uname not in allowed:
			await query.answer("Нет доступа", show_alert=True)
			return

		station_id = int(query.data.replace("info_", ""))
		await query.answer()

		# Получаем информацию о станции
		station_data = await api_client.get_station(station_id)
		
		if not station_data:
			if query.message:
				await query.message.answer("❌ Не удалось получить информацию о станции.")
			return

		# Формируем сообщение с информацией
		name = station_data.get("name", "—")
		description = station_data.get("description") or "—"
		latitude = station_data.get("latitude")
		longitude = station_data.get("longitude")
		
		
		# Адрес
		address_data = station_data.get("address")
		if address_data:
			full_address = address_data.get("full_address")
			if not full_address:
				# Собираем адрес из частей
				parts = []
				if address_data.get("street"):
					parts.append(address_data["street"])
				if address_data.get("house_number"):
					parts.append(address_data["house_number"])
				if address_data.get("city"):
					parts.append(address_data["city"])
				if address_data.get("region"):
					parts.append(address_data["region"])
				if address_data.get("postal_code"):
					parts.append(address_data["postal_code"])
				full_address = ", ".join(parts) if parts else "—"
		else:
			full_address = "—"
		
		# Формируем текст
		info_text = (
			f"ℹ️ Информация о станции\n\n"
			f"🔌 <b>{name}</b>\n"
			f"📝 {description}\n\n"
			f"📍 <b>Адрес:</b>\n{full_address}\n\n"
		)
		
		if latitude and longitude:
			info_text += f"🗺 <b>Координаты:</b>\n{latitude}, {longitude}"
		
		# Клавиатура
		kb = InlineKeyboardMarkup(inline_keyboard=[])
		
		# Добавляем кнопку для открытия карты, если есть координаты
		if latitude and longitude:
			# Яндекс.Карты
			yandex_url = f"https://yandex.ru/maps/?pt={longitude},{latitude}&z=17&l=map"
			# Google Maps
			google_url = f"https://www.google.com/maps?q={latitude},{longitude}"
			# 2GIS
			gis_url = f"https://2gis.ru/geo/{longitude},{latitude}"
			
			kb.inline_keyboard.append([
				InlineKeyboardButton(text="🗺 Яндекс.Карты", url=yandex_url),
			])
			kb.inline_keyboard.append([
				InlineKeyboardButton(text="🌍 Google Maps", url=google_url),
				InlineKeyboardButton(text="🟢 2ГИС", url=gis_url),
			])
		
		kb.inline_keyboard.append([
			InlineKeyboardButton(text="⬅️ Назад к станции", callback_data=f"station_{station_id}")
		])
		
		if query.message:
			try:
				await query.message.edit_text(info_text, reply_markup=kb, parse_mode="HTML")
			except Exception:
				await query.message.answer(info_text, reply_markup=kb, parse_mode="HTML")

	@dp.callback_query(lambda c: c.data and c.data.startswith("terminals_"))
	async def terminals_menu_callback(query: CallbackQuery) -> None:
		"""Показать меню управления терминалами с актуальным статусом."""
		uname = normalize_username(query.from_user.username)
		if uname not in allowed:
			await query.answer("Нет доступа", show_alert=True)
			return

		station_id = int(query.data.replace("terminals_", ""))
		station_name = await get_station_name(station_id)
		
		await query.answer()
		
		# Показываем сообщение о загрузке
		loading_text = (
			f"💳 Управление терминалами\n\n"
			f"🔌 Станция #{station_id} — {station_name}\n\n"
			f"⏳ Загрузка статуса..."
		)
		if query.message:
			try:
				await query.message.edit_text(loading_text)
			except Exception:
				pass
		
		# Запрашиваем статус терминалов
		response = await mqtt_service.send_command(
			station_id=station_id,
			command_type="get_terminals_status",
			timeout=15.0,
		)
		
		# Отладка: выводим полученный ответ
		print(f"[DEBUG] get_terminals_status response: {response}")
		
		# Парсим ответ
		terminal_a_enabled = True
		terminal_b_enabled = True
		free_charging_a = False
		free_charging_b = False
		meter_error_a = False
		meter_error_b = False
		status_text = ""
		meter_status = ""
		
		if response.get("status") == "executed":
			terminal_a_enabled = response.get("terminal_a_enabled", True)
			terminal_b_enabled = response.get("terminal_b_enabled", True)
			free_charging_a = response.get("free_charging_a", False)
			free_charging_b = response.get("free_charging_b", False)
			meter_error_a = response.get("meter_error_a", False) or response.get("disabled_by_meter_a", False)
			meter_error_b = response.get("meter_error_b", False) or response.get("disabled_by_meter_b", False)
			status_text = "✅ Статус получен"
			
			# Добавляем предупреждение о счётчиках
			if meter_error_a and meter_error_b:
				meter_status = (
					"\n\n⚠️ ВНИМАНИЕ: Счётчики A и B недоступны!\n"
					"Терминалы отключены автоматически.\n\n"
					"Проверьте подключение счётчиков электроэнергии."
				)
			elif meter_error_a:
				meter_status = (
					"\n\n⚠️ Счётчик A недоступен!\n"
					"Терминал A отключён автоматически."
				)
			elif meter_error_b:
				meter_status = (
					"\n\n⚠️ Счётчик B недоступен!\n"
					"Терминал B отключён автоматически."
				)
		elif response.get("status") == "timeout":
			# Станция не ответила - показываем сообщение с кнопкой повтора
			timeout_kb = InlineKeyboardMarkup(inline_keyboard=[
				[InlineKeyboardButton(text="🔄 Повторить попытку", callback_data=f"terminals_{station_id}")],
				[InlineKeyboardButton(text="⬅️ Назад к станции", callback_data=f"station_{station_id}")],
			])
			timeout_text = (
				f"⏱ Станция не отвечает\n\n"
				f"🔌 Станция #{station_id} — {station_name}\n\n"
				f"Не удалось получить статус терминалов.\n"
				f"Станция не ответила в течение 15 секунд.\n\n"
				f"Попробуйте повторить попытку."
			)
			if query.message:
				try:
					await query.message.edit_text(timeout_text, reply_markup=timeout_kb)
				except Exception:
					await query.message.answer(timeout_text, reply_markup=timeout_kb)
			return
		elif response.get("status") == "error":
			# Ошибка MQTT соединения - показываем сообщение с кнопкой повтора
			error_kb = InlineKeyboardMarkup(inline_keyboard=[
				[InlineKeyboardButton(text="🔄 Повторить попытку", callback_data=f"terminals_{station_id}")],
				[InlineKeyboardButton(text="⬅️ Назад к станции", callback_data=f"station_{station_id}")],
			])
			error_text = (
				f"❌ Ошибка соединения\n\n"
				f"🔌 Станция #{station_id} — {station_name}\n\n"
				f"💬 {response.get('message', 'Неизвестная ошибка')}\n\n"
				f"Попробуйте повторить попытку."
			)
			if query.message:
				try:
					await query.message.edit_text(error_text, reply_markup=error_kb)
				except Exception:
					await query.message.answer(error_text, reply_markup=error_kb)
			return
		else:
			status_text = "⚠️ Ошибка получения статуса"
		
		kb = build_terminals_keyboard(
			station_id,
			terminal_a_enabled=terminal_a_enabled,
			terminal_b_enabled=terminal_b_enabled,
			free_charging_a=free_charging_a,
			free_charging_b=free_charging_b,
			meter_error_a=meter_error_a,
			meter_error_b=meter_error_b,
		)

		# Формируем текст меню
		if meter_error_a and meter_error_b:
			# Оба счётчика недоступны - упрощённое сообщение
			menu_text = (
				f"💳 Управление терминалами\n\n"
				f"🔌 Станция #{station_id} — {station_name}\n\n"
				f"{status_text}{meter_status}"
			)
		else:
			# Есть хотя бы один рабочий терминал
			menu_text = (
				f"💳 Управление терминалами\n\n"
				f"🔌 Станция #{station_id} — {station_name}\n\n"
				f"Нажмите на кнопку для переключения режима.\n"
				f"✅/🔴 — статус терминала\n"
				f"🆓/💰 — режим зарядки\n\n"
				f"{status_text}{meter_status}"
			)
		
		if query.message:
			try:
				await query.message.edit_text(menu_text, reply_markup=kb)
			except Exception:
				await query.message.answer(menu_text, reply_markup=kb)

	# Команды терминалов — после выполнения возвращаемся в меню терминалов
	TERMINAL_COMMANDS = {
		"enable_terminal_a", "disable_terminal_a",
		"enable_terminal_b", "disable_terminal_b",
		"free_charging_a", "free_charging_b",
		"disable_free_charging_a", "disable_free_charging_b",
	}

	@dp.callback_query(lambda c: c.data and c.data.startswith("cmd_"))
	async def command_callback(query: CallbackQuery) -> None:
		"""Отправить команду на станцию через MQTT и дождаться ответа."""
		uname = normalize_username(query.from_user.username)
		if uname not in allowed:
			await query.answer("Нет доступа", show_alert=True)
			return

		# Парсим callback: cmd_{station_id}_{command_type}
		parts = query.data.split("_", 2)
		if len(parts) < 3:
			await query.answer("Ошибка", show_alert=True)
			return

		station_id = int(parts[1])
		command_type = parts[2]
		command_name = COMMAND_NAMES.get(command_type, command_type)
		command_emoji = COMMAND_EMOJIS.get(command_type, "📋")
		station_name = await get_station_name(station_id)
		
		# Проверяем, является ли это командой терминала
		is_terminal_command = command_type in TERMINAL_COMMANDS

		await query.answer()

		# Показываем ожидание
		if query.message:
			try:
				await query.message.edit_text(
					f"⏳ Отправляю команду...\n\n"
					f"{command_emoji} {command_name}\n"
					f"🔌 Станция #{station_id} — {station_name}\n\n"
					f"Ожидание ответа от ESP32..."
				)
			except Exception:
				pass

		# Отправляем команду через MQTT и ждём ответ
		response_data = await mqtt_service.send_command(
			station_id=station_id,
			command_type=command_type,
			timeout=15.0,
		)

		# Формируем ответ
		back_kb = InlineKeyboardMarkup(inline_keyboard=[
			[InlineKeyboardButton(text="⬅️ Назад к станции", callback_data=f"station_{station_id}")],
			[InlineKeyboardButton(text="🔌 Все станции", callback_data="stations_list")],
		])

		status = response_data.get("status", "unknown")

		if status == "executed":
			status_emoji = "✅"
			status_text = "Выполнено"
		elif status == "failed":
			status_emoji = "❌"
			status_text = "Ошибка"
		elif status == "timeout":
			# Таймаут
			response_text = (
				f"⏱ Таймаут ожидания ответа\n\n"
				f"{command_emoji} {command_name}\n"
				f"🔌 Станция #{station_id} — {station_name}\n\n"
				f"Станция не ответила за 15 сек.\n"
				f"Возможно станция оффлайн."
			)
			timeout_kb = InlineKeyboardMarkup(inline_keyboard=[
				[InlineKeyboardButton(text="💳 К терминалам", callback_data=f"terminals_{station_id}")] if is_terminal_command else [],
				[InlineKeyboardButton(text="⬅️ Назад к станции", callback_data=f"station_{station_id}")],
				[InlineKeyboardButton(text="🔌 Все станции", callback_data="stations_list")],
			])
			# Фильтруем пустые строки
			timeout_kb = InlineKeyboardMarkup(inline_keyboard=[row for row in timeout_kb.inline_keyboard if row])
			if query.message:
				try:
					await query.message.edit_text(response_text, reply_markup=timeout_kb)
				except Exception:
					await query.message.answer(response_text, reply_markup=timeout_kb)
			return
		elif status == "error":
			# Ошибка отправки
			error_text = (
				f"❌ Ошибка отправки команды\n\n"
				f"{command_emoji} {command_name}\n"
				f"🔌 Станция #{station_id} — {station_name}\n"
				f"💬 {response_data.get('message', 'Неизвестная ошибка')}"
			)
			if query.message:
				try:
					await query.message.edit_text(error_text, reply_markup=back_kb)
				except Exception:
					await query.message.answer(error_text, reply_markup=back_kb)
			return
		else:
			status_emoji = "📬"
			status_text = status

		# Для команд терминалов — возвращаемся в меню терминалов с обновлённым статусом
		if is_terminal_command and status == "executed":
			# Запрашиваем актуальный статус терминалов
			status_response = await mqtt_service.send_command(
				station_id=station_id,
				command_type="get_terminals_status",
				timeout=5.0,
			)
			
			terminal_a_enabled = status_response.get("terminal_a_enabled", True)
			terminal_b_enabled = status_response.get("terminal_b_enabled", True)
			free_charging_a = status_response.get("free_charging_a", False)
			free_charging_b = status_response.get("free_charging_b", False)
			meter_error_a = status_response.get("meter_error_a", False) or status_response.get("disabled_by_meter_a", False)
			meter_error_b = status_response.get("meter_error_b", False) or status_response.get("disabled_by_meter_b", False)
			
			kb = build_terminals_keyboard(
				station_id,
				terminal_a_enabled=terminal_a_enabled,
				terminal_b_enabled=terminal_b_enabled,
				free_charging_a=free_charging_a,
				free_charging_b=free_charging_b,
				meter_error_a=meter_error_a,
				meter_error_b=meter_error_b,
			)
			
			# Получаем сообщение от ESP
			message = response_data.get("message", "")
			
			menu_text = (
				f"💳 Управление терминалами\n\n"
				f"🔌 Станция #{station_id} — {station_name}\n\n"
				f"{status_emoji} {command_name}\n"
				f"{'💬 ' + message if message else ''}\n\n"
				f"Нажмите на кнопку для переключения режима."
			)
			
			if query.message:
				try:
					await query.message.edit_text(menu_text, reply_markup=kb)
				except Exception:
					await query.message.answer(menu_text, reply_markup=kb)
			return

		# Для остальных команд — стандартный ответ
		response_text = (
			f"{status_emoji} {command_emoji} {command_name}\n\n"
			f"🔌 Станция #{station_id} — {station_name}\n"
			f"📋 Статус: {status_text}\n"
		)

		# Парсим ответ от ESP32
		message = response_data.get("message")
		if message:
			response_text += f"\n💬 {message}"

		if query.message:
			try:
				await query.message.edit_text(response_text, reply_markup=back_kb)
			except Exception:
				await query.message.answer(response_text, reply_markup=back_kb)

	@dp.callback_query(lambda c: c.data in {"stats_today", "stats_week", "stats_month", "stats_custom"})
	async def stats_callbacks(query: CallbackQuery) -> None:
		if not query.message:
			return
		uname = normalize_username(query.from_user.username)
		if uname not in allowed:
			await query.answer("Нет доступа", show_alert=True)
			return

		orig_msg = query.message
		code = query.data or ""
		if code == "stats_custom":
			pending_custom_input.add(query.message.chat.id)
			try:
				await orig_msg.delete()
			except Exception:
				pass
			await query.message.answer(
				"Введите диапазон дат в формате:\n"
				"ДД.ММ.ГГГГ - ДД.ММ.ГГГГ\n"
				"Например: 01.11.2025 - 13.11.2025"
			)
			await query.answer()
			return

		date_from: datetime
		date_to: datetime
		offset_hours = int(os.getenv("BOT_TZ_OFFSET_HOURS", "7"))
		local_tz = timezone(timedelta(hours=offset_hours))
		now_local = datetime.now(local_tz)
		if code == "stats_today":
			start_local = now_local.replace(hour=0, minute=0, second=0, microsecond=0)
			end_local = start_local + timedelta(days=1)
			date_from = start_local.astimezone(timezone.utc)
			date_to = end_local.astimezone(timezone.utc)
			title = "за сегодня"
		elif code == "stats_week":
			start_local = now_local - timedelta(days=7)
			date_from = start_local.astimezone(timezone.utc)
			date_to = now_local.astimezone(timezone.utc)
			title = "за последние 7 дней"
		else:
			start_local = now_local - timedelta(days=30)
			date_from = start_local.astimezone(timezone.utc)
			date_to = now_local.astimezone(timezone.utc)
			title = "за последние 30 дней"

		conn_l = await asyncpg.connect(settings.database_dsn)
		try:
			stats = await aggregate_stats(conn_l, date_from, date_to)
		finally:
			await conn_l.close()

		try:
			await orig_msg.delete()
		except Exception:
			pass

		text = format_stats_message(
			title=title,
			date_from=date_from,
			date_to=date_to,
			total_paid=stats["total_paid"],
			total_refund=stats["total_refund"],
			profit=stats["profit"],
			total_kwh=stats["total_kwh"],
		)
		await query.message.answer(text)
		await query.answer()

	@dp.message()
	async def handle_custom_range(message: Message) -> None:
		txt = (message.text or "").strip()
		uname = normalize_username(message.from_user.username)

		# Кнопки главного меню
		if txt in {"📊 Статистика", "stats", "статистика"}:
			if uname not in allowed:
				await message.answer("У вас нет доступа к боту.")
				return
			kb = build_period_keyboard()
			await message.answer("Выберите период:", reply_markup=kb)
			return

		if txt in {"🔌 Станции", "stations", "станции"}:
			if uname not in allowed:
				await message.answer("У вас нет доступа к боту.")
				return
			stations = await api_client.get_stations()
			if not stations:
				await message.answer("❌ Станции не найдены или API недоступен.")
				return
			# Обновляем кэш названий
			for station in stations:
				stations_cache[station.get("id")] = station.get("name", f"Станция {station.get('id')}")
			kb = build_stations_keyboard(stations)
			await message.answer("🔌 Выберите станцию:", reply_markup=kb)
			return

		if txt.lower() in {"🔊 включить дебаг", "включить дебаг", "🔇 отключить дебаг", "отключить дебаг"}:
			if uname not in allowed:
				await message.answer("У вас нет доступа к боту.")
				return
			enable = "включить" in txt.lower()
			conn_l = await asyncpg.connect(settings.database_dsn)
			try:
				if enable:
					await upsert_debug_subscriber(conn_l, uname, message.chat.id)
				else:
					await remove_debug_subscriber(conn_l, uname)
			finally:
				await conn_l.close()
			msg = "✅ Подписка на отладочные сообщения включена." if enable else "🔇 Подписка на отладочные сообщения отключена."
			await message.answer(msg, reply_markup=build_main_keyboard(enable))
			return

		# Обработка произвольной даты
		if message.chat.id not in pending_custom_input:
			return
		if uname not in allowed:
			await message.answer("У вас нет доступа к боту.")
			return

		text = txt

		def parse_date(s: str) -> datetime | None:
			try:
				d = datetime.strptime(s, "%d.%m.%Y")
				offset_hours = int(os.getenv("BOT_TZ_OFFSET_HOURS", "7"))
				local_tz = timezone(timedelta(hours=offset_hours))
				return d.replace(tzinfo=local_tz)
			except Exception:
				return None

		date_from: datetime | None = None
		date_to: datetime | None = None
		if "-" in text:
			part1, part2 = [p.strip() for p in text.split("-", 1)]
			d1 = parse_date(part1)
			d2 = parse_date(part2)
			if d1 and d2 and d2 >= d1:
				date_from = d1.astimezone(timezone.utc)
				date_to = (d2 + timedelta(days=1)).astimezone(timezone.utc)
		else:
			d = parse_date(text)
			if d:
				date_from = d.astimezone(timezone.utc)
				date_to = (d + timedelta(days=1)).astimezone(timezone.utc)

		if not date_from or not date_to:
			await message.answer(
				"Не удалось распознать дату. Используйте формат:\n"
				"ДД.ММ.ГГГГ - ДД.ММ.ГГГГ\n"
				"Например: 01.11.2025 - 13.11.2025"
			)
			return

		conn_l = await asyncpg.connect(settings.database_dsn)
		try:
			stats = await aggregate_stats(conn_l, date_from, date_to)
		finally:
			await conn_l.close()

		resp = format_stats_message(
			title="за выбранный период",
			date_from=date_from,
			date_to=date_to,
			total_paid=stats["total_paid"],
			total_refund=stats["total_refund"],
			profit=stats["profit"],
			total_kwh=stats["total_kwh"],
		)
		await message.answer(resp)
		with suppress(KeyError):
			pending_custom_input.remove(message.chat.id)

	# Start MQTT service
	await mqtt_service.start()

	# Run listeners in background
	listener_task = asyncio.create_task(listen_and_broadcast(bot, settings.database_dsn))
	debug_listener_task = asyncio.create_task(listen_and_broadcast_debug(bot, settings.database_dsn))
	try:
		await dp.start_polling(bot)
	finally:
		listener_task.cancel()
		debug_listener_task.cancel()
		with suppress(asyncio.CancelledError):
			await listener_task
		with suppress(asyncio.CancelledError):
			await debug_listener_task
		await mqtt_service.stop()
		await api_client.close()


if __name__ == "__main__":
	asyncio.run(main())
