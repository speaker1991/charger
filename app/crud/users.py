"""CRUD operations for Users."""
import json
from sqlalchemy.orm import Session
from .. import models
from ..security import hash_password


def get_user_by_email(db: Session, email: str) -> models.User | None:
	return db.query(models.User).filter(models.User.email == email).first()


def get_user_by_id(db: Session, user_id: int) -> models.User | None:
	return db.query(models.User).filter(models.User.id == user_id).first()


def create_user(
	db: Session,
	email: str,
	password: str,
	full_name: str,
	phone: str | None,
	role: str,
	owner_id: int | None = None,
	station_ids: list[int] | None = None,
) -> models.User:
	user = models.User(
		email=email,
		password_hash=hash_password(password),
		full_name=full_name,
		phone=phone,
		role=role,
		owner_id=owner_id,
		is_active=1,
	)
	db.add(user)
	db.flush()  # get user.id

	# Create station access records
	if station_ids:
		for sid in station_ids:
			access = models.UserStationAccess(user_id=user.id, station_id=sid)
			db.add(access)

	db.commit()
	db.refresh(user)
	return user


def list_users(db: Session, role: str | None = None, owner_id: int | None = None) -> list[models.User]:
	q = db.query(models.User)
	if role:
		q = q.filter(models.User.role == role)
	if owner_id is not None:
		q = q.filter(models.User.owner_id == owner_id)
	return q.order_by(models.User.created_at.desc()).all()


def toggle_user_active(db: Session, user_id: int, is_active: bool) -> models.User | None:
	user = get_user_by_id(db, user_id)
	if not user:
		return None
	user.is_active = 1 if is_active else 0
	db.commit()
	db.refresh(user)
	return user


def get_user_station_ids(db: Session, user_id: int) -> list[int]:
	"""Get list of station IDs accessible to user."""
	records = db.query(models.UserStationAccess).filter(
		models.UserStationAccess.user_id == user_id
	).all()
	return [r.station_id for r in records]


def get_accessible_station_ids(db: Session, user: models.User) -> list[int] | None:
	"""
	Returns list of station IDs user can access, or None if user has access to ALL (super_admin).
	"""
	if user.role == "super_admin":
		return None  # access to all
	return get_user_station_ids(db, user.id)
