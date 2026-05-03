"""
MQTT Service for publishing commands to ESP32 stations.

Topic structure:
- stations/{station_id}/commands  - commands from server to ESP32
- stations/{station_id}/status    - status updates from ESP32 to server
- stations/{station_id}/response  - command responses from ESP32
"""

import asyncio
import json
import logging
import uuid
from contextlib import suppress
from datetime import datetime, timezone
from typing import Any
from enum import Enum

import aiomqtt

from .config import settings

logger = logging.getLogger(__name__)


class CommandType(str, Enum):
    """Типы команд для ESP32."""
    GET_STATUS = "get_status"           # Запросить статус станции
    STOP_CHARGING = "stop_charging"     # Остановить зарядку
    START_CHARGING = "start_charging"   # Начать зарядку
    RESTART = "restart"                 # Перезагрузить ESP32
    SET_MAX_CURRENT = "set_max_current" # Установить макс. ток
    PING = "ping"                       # Проверка связи


class MQTTService:
    """
    Сервис для работы с MQTT брокером.
    
    Использует persistent connection для минимизации задержек.
    Автоматически переподключается при обрыве связи.
    """

    def __init__(self, host: str, port: int, username: str | None = None, password: str | None = None):
        self.host = host
        self.port = port
        self.username = username
        self.password = password
        self._client: aiomqtt.Client | None = None
        self._lock = asyncio.Lock()
        self._connected = asyncio.Event()
        self._reconnect_task: asyncio.Task | None = None
        self._running = False

    async def start(self) -> None:
        """Запустить persistent MQTT connection."""
        if self._running:
            return
        self._running = True
        self._reconnect_task = asyncio.create_task(self._connection_loop())
        # Ждём первого подключения (макс 5 сек)
        try:
            await asyncio.wait_for(self._connected.wait(), timeout=5.0)
            logger.info("MQTT Service started with persistent connection")
        except asyncio.TimeoutError:
            logger.warning("MQTT initial connection timeout, will retry in background")

    async def stop(self) -> None:
        """Остановить MQTT сервис."""
        self._running = False
        if self._reconnect_task:
            self._reconnect_task.cancel()
            with suppress(asyncio.CancelledError):
                await self._reconnect_task
        self._client = None
        self._connected.clear()
        logger.info("MQTT Service stopped")

    async def _connection_loop(self) -> None:
        """Цикл поддержания persistent connection."""
        while self._running:
            try:
                async with aiomqtt.Client(
                    hostname=self.host,
                    port=self.port,
                    identifier=f"api-server-{uuid.uuid4().hex[:8]}",
                    username=self.username,
                    password=self.password,
                    keepalive=30,  # Быстрое обнаружение обрыва
                ) as client:
                    self._client = client
                    self._connected.set()
                    logger.info(f"MQTT connected to {self.host}:{self.port}")
                    
                    # Держим соединение открытым
                    while self._running:
                        await asyncio.sleep(1)
                        
            except aiomqtt.MqttError as e:
                logger.error(f"MQTT connection error: {e}")
                self._client = None
                self._connected.clear()
                if self._running:
                    await asyncio.sleep(2)  # Быстрый reconnect
            except asyncio.CancelledError:
                break
            except Exception as e:
                logger.error(f"MQTT unexpected error: {e}")
                self._client = None
                self._connected.clear()
                if self._running:
                    await asyncio.sleep(2)

    async def publish_command(
        self,
        station_id: int,
        command_type: CommandType,
        payload: dict[str, Any] | None = None,
        command_id: str | None = None,
    ) -> bool:
        """
        Опубликовать команду для станции.
        
        Использует persistent connection для минимальной задержки.

        Args:
            station_id: ID станции
            command_type: Тип команды
            payload: Дополнительные данные команды
            command_id: UUID команды для отслеживания

        Returns:
            True если успешно опубликовано
        """
        if not self._connected.is_set() or not self._client:
            logger.warning("MQTT not connected, waiting...")
            try:
                await asyncio.wait_for(self._connected.wait(), timeout=3.0)
            except asyncio.TimeoutError:
                logger.error("MQTT connection timeout")
                return False

        topic = f"stations/{station_id}/commands"
        message = {
            "command": command_type.value,
            "command_id": command_id,
            "timestamp": datetime.now(timezone.utc).isoformat(),
            "payload": payload or {},
        }

        try:
            async with self._lock:
                await self._client.publish(
                    topic,
                    json.dumps(message),
                    qos=1,  # At least once delivery
                )
            logger.info(f"Published command {command_type} to station {station_id}")
            return True
        except Exception as e:
            logger.error(f"Failed to publish command: {e}")
            return False

    async def health_check(self) -> bool:
        """Проверить подключение к MQTT брокеру."""
        return self._connected.is_set() and self._client is not None


# Глобальный экземпляр сервиса
mqtt_service = MQTTService(
    host=settings.mqtt_host,
    port=settings.mqtt_port,
    username=getattr(settings, 'mqtt_username', None),
    password=getattr(settings, 'mqtt_password', None),
)


# Dependency для FastAPI
async def get_mqtt_service() -> MQTTService:
    """Dependency для получения MQTT сервиса."""
    return mqtt_service

