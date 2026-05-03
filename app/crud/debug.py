from typing import Optional
from sqlalchemy.orm import Session

from .. import models, schemas


def create_debug_log(db: Session, station_id: int, data: schemas.EspDebugCreate) -> models.EspDebugLog:
	payload = models.EspDebugLog(
		station_id=station_id,
		occurred_at=data.occurred_at,
		text1=data.text1,
		text2=data.text2,
		text3=data.text3,
		text4=data.text4,
		text5=data.text5,
		text6=data.text6,
	)
	db.add(payload)
	db.commit()
	db.refresh(payload)
	return payload


