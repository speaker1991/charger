"""CRUD operations for Invites."""
import json
import secrets
from sqlalchemy.orm import Session
from .. import models


def create_invite(
	db: Session,
	target_role: str,
	created_by: int,
	station_ids: list[int] | None = None,
) -> models.Invite:
	token = secrets.token_urlsafe(32)
	invite = models.Invite(
		token=token,
		target_role=target_role,
		station_ids=json.dumps(station_ids) if station_ids else None,
		created_by=created_by,
		is_used=0,
	)
	db.add(invite)
	db.commit()
	db.refresh(invite)
	return invite


def get_invite_by_token(db: Session, token: str) -> models.Invite | None:
	return db.query(models.Invite).filter(models.Invite.token == token).first()


def mark_invite_used(db: Session, invite: models.Invite, used_by_id: int) -> None:
	invite.is_used = 1
	invite.used_by = used_by_id
	db.commit()


def list_invites(db: Session, created_by: int | None = None) -> list[models.Invite]:
	q = db.query(models.Invite)
	if created_by is not None:
		q = q.filter(models.Invite.created_by == created_by)
	return q.order_by(models.Invite.created_at.desc()).all()
