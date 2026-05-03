"""CRUD operations for station commands."""

import json
import uuid
from datetime import datetime, timezone
from typing import List

from sqlalchemy.orm import Session

from ..models import StationCommand, Station
from ..schemas import StationCommandCreate


def create_command(
    db: Session,
    station_id: int,
    data: StationCommandCreate,
) -> StationCommand:
    """Создать запись о команде."""
    command_id = str(uuid.uuid4())
    payload_json = json.dumps(data.payload) if data.payload else None

    command = StationCommand(
        command_id=command_id,
        station_id=station_id,
        command_type=data.command_type,
        payload=payload_json,
        status="pending",
        sent_by=data.sent_by,
    )
    db.add(command)
    db.commit()
    db.refresh(command)
    return command


def update_command_status(
    db: Session,
    command_id: str,
    status: str,
    response: dict | None = None,
) -> StationCommand | None:
    """Обновить статус команды."""
    command = db.query(StationCommand).filter(
        StationCommand.command_id == command_id
    ).first()

    if not command:
        return None

    command.status = status
    if status == "sent":
        command.sent_at = datetime.now(timezone.utc)
    elif status in ("executed", "failed", "delivered"):
        command.responded_at = datetime.now(timezone.utc)

    if response:
        command.response = json.dumps(response)

    db.commit()
    db.refresh(command)
    return command


def get_command(db: Session, command_id: str) -> StationCommand | None:
    """Получить команду по ID."""
    return db.query(StationCommand).filter(
        StationCommand.command_id == command_id
    ).first()


def get_command_by_pk(db: Session, pk: int) -> StationCommand | None:
    """Получить команду по первичному ключу."""
    return db.query(StationCommand).filter(
        StationCommand.id == pk
    ).first()


def list_commands(
    db: Session,
    station_id: int | None = None,
    status: str | None = None,
    skip: int = 0,
    limit: int = 100,
) -> List[StationCommand]:
    """Получить список команд с фильтрацией."""
    query = db.query(StationCommand)

    if station_id is not None:
        query = query.filter(StationCommand.station_id == station_id)
    if status is not None:
        query = query.filter(StationCommand.status == status)

    return query.order_by(StationCommand.created_at.desc()).offset(skip).limit(limit).all()


def get_pending_commands(db: Session, station_id: int) -> List[StationCommand]:
    """Получить pending команды для станции (для polling если нужно)."""
    return db.query(StationCommand).filter(
        StationCommand.station_id == station_id,
        StationCommand.status == "pending",
    ).order_by(StationCommand.created_at.asc()).all()

