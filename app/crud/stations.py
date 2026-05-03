from typing import List, Optional

from sqlalchemy.orm import Session

from .. import models, schemas
from ..security import generate_station_api_key, station_key_fingerprint_hex


def create_station(db: Session, data: schemas.StationCreate) -> tuple[models.Station, str]:
	plaintext_key, salt_hex, hash_hex = generate_station_api_key()
	fingerprint_hex = station_key_fingerprint_hex(plaintext_key)
	station = models.Station(
		name=data.name,
		description=data.description,
		latitude=data.latitude,
		longitude=data.longitude,
		address_id=data.address_id,
		api_key_salt=salt_hex,
		api_key_hash=hash_hex,
		api_key_fingerprint=fingerprint_hex,
	)
	db.add(station)
	db.commit()
	db.refresh(station)
	return station, plaintext_key


def list_stations(db: Session, skip: int = 0, limit: int = 100) -> List[models.Station]:
	return (
		db.query(models.Station)
		.order_by(models.Station.id.desc())
		.offset(skip)
		.limit(limit)
		.all()
	)


def get_station(db: Session, station_id: int) -> Optional[models.Station]:
	return db.query(models.Station).filter(models.Station.id == station_id).first()


def rotate_station_key(db: Session, station_id: int) -> Optional[tuple[models.Station, str]]:
	station = get_station(db, station_id)
	if not station:
		return None
	plaintext_key, salt_hex, hash_hex = generate_station_api_key()
	station.api_key_salt = salt_hex
	station.api_key_hash = hash_hex
	station.api_key_fingerprint = station_key_fingerprint_hex(plaintext_key)
	db.add(station)
	db.commit()
	db.refresh(station)
	return station, plaintext_key


