"""CRUD operations for Free Charging Sessions."""
from datetime import datetime, timezone
from sqlalchemy.orm import Session
from sqlalchemy import func
from .. import models


def start_free_charging(db: Session, user_id: int, station_id: int) -> models.FreeChargingSession:
	session = models.FreeChargingSession(
		user_id=user_id,
		station_id=station_id,
		started_at=datetime.now(timezone.utc),
		kwh_spent=0.0,
		is_active=1,
	)
	db.add(session)
	db.commit()
	db.refresh(session)
	return session


def get_active_session(db: Session, station_id: int) -> models.FreeChargingSession | None:
	"""Get active free charging session for a station."""
	return db.query(models.FreeChargingSession).filter(
		models.FreeChargingSession.station_id == station_id,
		models.FreeChargingSession.is_active == 1,
	).first()


def finish_free_charging(db: Session, session_id: int, kwh_spent: float) -> models.FreeChargingSession | None:
	session = db.query(models.FreeChargingSession).filter(
		models.FreeChargingSession.id == session_id
	).first()
	if not session:
		return None
	session.is_active = 0
	session.kwh_spent = kwh_spent
	session.finished_at = datetime.now(timezone.utc)
	db.commit()
	db.refresh(session)
	return session


def update_session_kwh(db: Session, session_id: int, kwh_spent: float) -> None:
	"""Update kWh for active session (called from MQTT ingest)."""
	session = db.query(models.FreeChargingSession).filter(
		models.FreeChargingSession.id == session_id
	).first()
	if session:
		session.kwh_spent = kwh_spent
		db.commit()


def get_employee_stats(
	db: Session,
	date_from: datetime | None = None,
	date_to: datetime | None = None,
	owner_id: int | None = None,
) -> list[dict]:
	"""Get free charging statistics grouped by employee."""
	q = db.query(
		models.FreeChargingSession.user_id,
		models.User.full_name,
		models.User.email,
		func.count(models.FreeChargingSession.id).label("session_count"),
		func.coalesce(func.sum(models.FreeChargingSession.kwh_spent), 0).label("total_kwh"),
	).join(
		models.User, models.FreeChargingSession.user_id == models.User.id
	)

	if date_from:
		q = q.filter(models.FreeChargingSession.started_at >= date_from)
	if date_to:
		q = q.filter(models.FreeChargingSession.started_at <= date_to)
	if owner_id is not None:
		q = q.filter(models.User.owner_id == owner_id)

	rows = q.group_by(
		models.FreeChargingSession.user_id,
		models.User.full_name,
		models.User.email,
	).all()

	return [
		{
			"user_id": r.user_id,
			"full_name": r.full_name,
			"email": r.email,
			"session_count": r.session_count,
			"total_kwh": float(r.total_kwh),
		}
		for r in rows
	]
