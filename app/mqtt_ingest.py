"""
MQTT Ingest Service - слушает данные от ESP32 станций и записывает в БД.

Топики:
- stations/{station_id}/status   - периодическая телеметрия (прямое подключение)
- stations/{station_id}/debug    - отладочные сообщения
- stations/{station_id}/payments - информация о платежах

Станции подключаются напрямую к локальному брокеру по статическому IP.
Мост с 92.53.64.13 decommissioned 2026-05-02.

Уведомления в Telegram бот отправляются через PostgreSQL триггеры:
- trg_payment_transactions_notify → payment_transactions_channel
- trg_esp_debug_notify → esp_debug_channel
"""

import asyncio
import json
import logging
import re
import uuid
from datetime import datetime, timezone
from contextlib import suppress
from decimal import Decimal

import aiomqtt

from .config import settings
from .database import SessionLocal
from . import models

logger = logging.getLogger(__name__)


class MQTTIngestService:
    """Сервис для приёма данных от станций через MQTT."""

    def __init__(self, host: str, port: int):
        self.host = host
        self.port = port
        self._task: asyncio.Task | None = None
        self._running = False

    async def start(self) -> None:
        """Запустить MQTT ingest listener."""
        if self._running:
            return
        self._running = True
        self._task = asyncio.create_task(self._listener_loop())
        logger.info("MQTT Ingest Service started")

    async def stop(self) -> None:
        """Остановить MQTT ingest listener."""
        self._running = False
        if self._task:
            self._task.cancel()
            with suppress(asyncio.CancelledError):
                await self._task
        logger.info("MQTT Ingest Service stopped")

    async def _listener_loop(self) -> None:
        """Основной цикл прослушивания MQTT топиков."""
        while self._running:
            try:
                async with aiomqtt.Client(
                    hostname=self.host,
                    port=self.port,
                    identifier=f"api-ingest-{uuid.uuid4().hex[:8]}",
                    keepalive=30,
                ) as client:
                    # Подписываемся на топики от всех станций
                    await client.subscribe("stations/+/status")
                    await client.subscribe("stations/+/debug")
                    await client.subscribe("stations/+/payments")
                    logger.info("MQTT Ingest: Subscribed to stations/+/status, +/debug and +/payments")

                    async for message in client.messages:
                        try:
                            await self._handle_message(message)
                        except Exception as e:
                            logger.error(f"MQTT Ingest: Error handling message: {e}")

            except aiomqtt.MqttError as e:
                logger.error(f"MQTT Ingest connection error: {e}")
                if self._running:
                    await asyncio.sleep(5)
            except asyncio.CancelledError:
                break
            except Exception as e:
                logger.error(f"MQTT Ingest unexpected error: {e}")
                if self._running:
                    await asyncio.sleep(5)

    async def _handle_message(self, message: aiomqtt.Message) -> None:
        """Обработать входящее MQTT сообщение."""
        topic = str(message.topic)
        
        # Парсим station_id из топика: stations/{id}/status, debug или payments
        match = re.match(r"stations/(\d+)/(status|debug|payments)", topic)
        if not match:
            logger.warning(f"MQTT Ingest: Unknown topic format: {topic}")
            return

        station_id = int(match.group(1))
        message_type = match.group(2)

        try:
            payload = json.loads(message.payload.decode())
        except json.JSONDecodeError as e:
            logger.error(f"MQTT Ingest: Invalid JSON: {e}")
            return

        if message_type == "status":
            logger.info(f"MQTT Ingest: Received STATUS from station {station_id}")
            await self._handle_status(station_id, payload)
        elif message_type == "debug":
            logger.info(f"MQTT Ingest: Received DEBUG from station {station_id}")
            await self._handle_debug(station_id, payload)
        elif message_type == "payments":
            logger.info(f"MQTT Ingest: Received PAYMENT from station {station_id}: {payload}")
            await self._handle_payment(station_id, payload)

    async def _handle_status(self, station_id: int, payload: dict) -> None:
        """Обработать status телеметрию от станции и записать в esp_debug_logs.

        Формат от ESP32 прошивки (publishStatus):
          station_id, charging_active, current_power, current_kwh,
          pistol, uptime_seconds, free_heap, wifi_rssi
        """
        db = SessionLocal()
        try:
            # Структурируем телеметрию в text1-text6 для сохранения в debug log
            debug_log = models.EspDebugLog(
                station_id=station_id,
                occurred_at=datetime.now(timezone.utc),
                text1=f"power={payload.get('current_power', 0)}kW  kwh={payload.get('current_kwh', 0)}",
                text2=f"charging={'ON' if payload.get('charging_active') else 'OFF'}  pistol={payload.get('pistol', '-')}",
                text3=f"uptime={payload.get('uptime_seconds', 0)}s  heap={payload.get('free_heap', 0)}B",
                text4=f"wifi_rssi={payload.get('wifi_rssi', 0)}dBm",
                text5="source=direct_mqtt_status",
                text6="",
            )
            db.add(debug_log)
            db.commit()
            logger.debug(f"MQTT Ingest: Status telemetry saved for station {station_id}")
        except Exception as e:
            logger.error(f"MQTT Ingest: Error saving status telemetry: {e}")
            db.rollback()
        finally:
            db.close()

    async def _handle_debug(self, station_id: int, payload: dict) -> None:
        """Сохранить debug сообщение в БД (NOTIFY через PostgreSQL триггер)."""
        db = SessionLocal()
        try:
            debug_log = models.EspDebugLog(
                station_id=station_id,
                occurred_at=datetime.now(timezone.utc),
                text1=payload.get("text1", ""),
                text2=payload.get("text2", ""),
                text3=payload.get("text3", ""),
                text4=payload.get("text4", ""),
                text5=payload.get("text5", ""),
                text6=payload.get("text6", ""),
            )
            db.add(debug_log)
            db.commit()
            logger.debug(f"MQTT Ingest: Debug saved for station {station_id}")
        except Exception as e:
            logger.error(f"MQTT Ingest: Error saving debug: {e}")
            db.rollback()
        finally:
            db.close()

    async def _handle_payment(self, station_id: int, payload: dict) -> None:
        """Сохранить платёж в БД (NOTIFY через PostgreSQL триггер)."""
        db = SessionLocal()
        try:
            # Парсим occurred_at или используем текущее время сервера
            occurred_at_str = payload.get("occurred_at", "")
            occurred_at = None
            
            if occurred_at_str:
                try:
                    occurred_at = datetime.fromisoformat(occurred_at_str.replace("Z", "+00:00"))
                    # Проверяем что дата валидная (>= 2020 год)
                    # Если ESP не синхронизирован с NTP, он может прислать 1970 год
                    if occurred_at.year < 2020:
                        logger.warning(f"MQTT Ingest: Invalid date from station {station_id}: {occurred_at_str}, using server time")
                        occurred_at = None
                except ValueError:
                    logger.warning(f"MQTT Ingest: Cannot parse date: {occurred_at_str}")
            
            if occurred_at is None:
                occurred_at = datetime.now(timezone.utc)

            amount_paid = Decimal(str(payload.get("amount_paid", 0)))
            refund_amount = Decimal(str(payload.get("refund_amount", 0)))
            kwh_spent = float(payload.get("kwh_spent", 0))
            pistol = payload.get("pistol", "")

            payment = models.PaymentTransaction(
                station_id=station_id,
                occurred_at=occurred_at,
                amount_paid=amount_paid,
                refund_amount=refund_amount,
                kwh_spent=kwh_spent,
                pistol=pistol,
            )
            db.add(payment)
            db.commit()
            
            logger.info(
                f"MQTT Ingest: Payment saved for station {station_id}: "
                f"paid={amount_paid}, refund={refund_amount}, "
                f"kwh={kwh_spent}, pistol={pistol}"
            )
        except Exception as e:
            logger.error(f"MQTT Ingest: Error saving payment: {e}")
            db.rollback()
        finally:
            db.close()


# Глобальный экземпляр
mqtt_ingest = MQTTIngestService(
    host=settings.mqtt_host,
    port=settings.mqtt_port,
)
