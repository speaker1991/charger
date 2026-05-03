from typing import List, Optional

from sqlalchemy.orm import Session

from .. import models, schemas


def create_address(db: Session, data: schemas.AddressCreate) -> models.Address:
	address = models.Address(
		country_code=data.country_code,
		postal_code=data.postal_code,
		region=data.region,
		district=data.district,
		city=data.city,
		street=data.street,
		house_number=data.house_number,
		building=data.building,
		apartment=data.apartment,
		full_address=data.full_address,
	)
	db.add(address)
	db.commit()
	db.refresh(address)
	return address


def list_addresses(db: Session, skip: int = 0, limit: int = 100) -> List[models.Address]:
	return (
		db.query(models.Address)
		.order_by(models.Address.id.desc())
		.offset(skip)
		.limit(limit)
		.all()
	)


def get_address(db: Session, address_id: int) -> Optional[models.Address]:
	return db.query(models.Address).filter(models.Address.id == address_id).first()


def update_address(db: Session, address_id: int, data: schemas.AddressUpdate) -> Optional[models.Address]:
	address = get_address(db, address_id)
	if not address:
		return None
	if data.country_code is not None:
		address.country_code = data.country_code
	if data.postal_code is not None:
		address.postal_code = data.postal_code
	if data.region is not None:
		address.region = data.region
	if data.district is not None:
		address.district = data.district
	if data.city is not None:
		address.city = data.city
	if data.street is not None:
		address.street = data.street
	if data.house_number is not None:
		address.house_number = data.house_number
	if data.building is not None:
		address.building = data.building
	if data.apartment is not None:
		address.apartment = data.apartment
	if data.full_address is not None:
		address.full_address = data.full_address
	db.add(address)
	db.commit()
	db.refresh(address)
	return address


def delete_address(db: Session, address_id: int) -> bool:
	address = get_address(db, address_id)
	if not address:
		return False
	db.delete(address)
	db.commit()
	return True


