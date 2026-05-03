from typing import List, Optional
from datetime import datetime

from sqlalchemy.orm import Session
from sqlalchemy import and_

from .. import models, schemas


def create_payment_transaction(db: Session, station_id: int, data: schemas.PaymentTransactionCreate) -> models.PaymentTransaction:
	occurred_at = data.occurred_at or datetime.utcnow()
	pmt = models.PaymentTransaction(
		station_id=station_id,
		occurred_at=occurred_at,
		amount_paid=data.amount_paid,
		refund_amount=data.refund_amount,
		kwh_spent=data.kwh_spent,
		pistol=data.pistol,
	)
	db.add(pmt)
	db.commit()
	db.refresh(pmt)
	return pmt


def get_payment_transaction(db: Session, payment_id: int) -> Optional[models.PaymentTransaction]:
	return db.query(models.PaymentTransaction).filter(models.PaymentTransaction.id == payment_id).first()


def list_payment_transactions(
	db: Session,
	*,
	skip: int = 0,
	limit: int = 100,
	station_id: Optional[int] = None,
	date_from: Optional[datetime] = None,
	date_to: Optional[datetime] = None,
) -> List[models.PaymentTransaction]:
	query = db.query(models.PaymentTransaction)
	conditions = []
	if station_id is not None:
		conditions.append(models.PaymentTransaction.station_id == station_id)
	if date_from is not None:
		conditions.append(models.PaymentTransaction.occurred_at >= date_from)
	if date_to is not None:
		conditions.append(models.PaymentTransaction.occurred_at <= date_to)
	if conditions:
		query = query.filter(and_(*conditions))
	return (
		query.order_by(models.PaymentTransaction.id.desc())
		.offset(skip)
		.limit(limit)
		.all()
	)


