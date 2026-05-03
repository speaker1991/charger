"""
Security module — JWT-based auth + legacy API key support.
"""
from datetime import datetime, timedelta, timezone
from typing import Optional

from fastapi import HTTPException, Security, Depends, Request
from fastapi.security import HTTPBearer, HTTPAuthorizationCredentials
from fastapi.security.api_key import APIKeyHeader
from starlette.status import HTTP_401_UNAUTHORIZED, HTTP_403_FORBIDDEN
from sqlalchemy.orm import Session
from passlib.context import CryptContext
import jwt
import hmac
import os
import secrets
import hashlib

from .config import settings
from .database import get_db
from . import models

# ===== Password hashing =====
pwd_context = CryptContext(schemes=["bcrypt"], deprecated="auto")


def hash_password(password: str) -> str:
	return pwd_context.hash(password)


def verify_password(plain_password: str, hashed_password: str) -> bool:
	return pwd_context.verify(plain_password, hashed_password)


# ===== JWT tokens =====
def create_access_token(data: dict, expires_delta: Optional[timedelta] = None) -> str:
	to_encode = data.copy()
	expire = datetime.now(timezone.utc) + (expires_delta or timedelta(minutes=settings.jwt_expire_minutes))
	to_encode.update({"exp": expire})
	return jwt.encode(to_encode, settings.jwt_secret_key, algorithm=settings.jwt_algorithm)


def decode_access_token(token: str) -> dict:
	try:
		return jwt.decode(token, settings.jwt_secret_key, algorithms=[settings.jwt_algorithm])
	except jwt.ExpiredSignatureError:
		raise HTTPException(status_code=HTTP_401_UNAUTHORIZED, detail="Token expired")
	except jwt.InvalidTokenError:
		raise HTTPException(status_code=HTTP_401_UNAUTHORIZED, detail="Invalid token")


# ===== Auth dependencies =====
_bearer_scheme = HTTPBearer(auto_error=False)
_api_key_header = APIKeyHeader(name="X-API-Key", auto_error=False, scheme_name="AdminKey")
_station_key_header = APIKeyHeader(name="X-Station-Key", auto_error=False, scheme_name="StationKey")


def get_current_user(
	credentials: HTTPAuthorizationCredentials | None = Security(_bearer_scheme),
	db: Session = Depends(get_db),
) -> models.User:
	"""Extract and validate JWT, return User object."""
	if credentials is None:
		raise HTTPException(status_code=HTTP_401_UNAUTHORIZED, detail="Not authenticated")
	payload = decode_access_token(credentials.credentials)
	user_id = payload.get("sub")
	if user_id is None:
		raise HTTPException(status_code=HTTP_401_UNAUTHORIZED, detail="Invalid token payload")
	user = db.query(models.User).filter(models.User.id == int(user_id)).first()
	if not user:
		raise HTTPException(status_code=HTTP_401_UNAUTHORIZED, detail="User not found")
	if not user.is_active:
		raise HTTPException(status_code=HTTP_403_FORBIDDEN, detail="Account is blocked")
	return user


def require_role(*roles: str):
	"""Dependency factory: require user to have one of the given roles."""
	def checker(user: models.User = Depends(get_current_user)) -> models.User:
		if user.role not in roles:
			raise HTTPException(status_code=HTTP_403_FORBIDDEN, detail="Insufficient permissions")
		return user
	return checker


# ===== Legacy API key auth (for stations/bots) =====
def require_api_key(api_key: str | None = Security(_api_key_header)) -> str:
	if api_key is None or not hmac.compare_digest(api_key, settings.api_key):
		raise HTTPException(
			status_code=HTTP_401_UNAUTHORIZED,
			detail="Invalid or missing API key",
		)
	return api_key


# ===== Station API key auth =====
def _pbkdf2_sha256_hex(secret: str, salt_hex: str, iterations: int = 200_000) -> str:
	salt = bytes.fromhex(salt_hex)
	dk = hashlib.pbkdf2_hmac("sha256", secret.encode("utf-8"), salt, iterations)
	return dk.hex()


def station_key_fingerprint_hex(plaintext_key: str) -> str:
	return hashlib.sha256(plaintext_key.encode("utf-8")).hexdigest()


def generate_station_api_key() -> tuple[str, str, str]:
	plaintext_key = secrets.token_urlsafe(32)
	salt_hex = os.urandom(16).hex()
	hash_hex = _pbkdf2_sha256_hex(plaintext_key, salt_hex)
	return plaintext_key, salt_hex, hash_hex


def verify_station_api_key(plaintext_key: str, salt_hex: str, expected_hash_hex: str) -> bool:
	actual_hash_hex = _pbkdf2_sha256_hex(plaintext_key, salt_hex)
	return hmac.compare_digest(actual_hash_hex, expected_hash_hex)


def require_station(
	station_key: str | None = Security(_station_key_header),
	db: Session = Depends(get_db),
) -> models.Station:
	if not station_key:
		raise HTTPException(status_code=HTTP_401_UNAUTHORIZED, detail="Missing station key")
	fp = station_key_fingerprint_hex(station_key)
	station = (
		db.query(models.Station)
		.filter(models.Station.api_key_fingerprint == fp)
		.first()
	)
	if not station or not verify_station_api_key(station_key, station.api_key_salt, station.api_key_hash):
		raise HTTPException(status_code=HTTP_401_UNAUTHORIZED, detail="Invalid station key")
	return station


def require_admin_or_station(
	api_key: str | None = Security(_api_key_header),
	station_key: str | None = Security(_station_key_header),
	db: Session = Depends(get_db),
) -> str:
	if api_key is not None and hmac.compare_digest(api_key, settings.api_key):
		return "admin"
	if station_key:
		fp = station_key_fingerprint_hex(station_key)
		station = (
			db.query(models.Station)
			.filter(models.Station.api_key_fingerprint == fp)
			.first()
		)
		if station and verify_station_api_key(station_key, station.api_key_salt, station.api_key_hash):
			return "station"
	raise HTTPException(status_code=HTTP_401_UNAUTHORIZED, detail="Invalid or missing credentials")


def require_jwt_or_api_key(
	credentials: HTTPAuthorizationCredentials | None = Security(_bearer_scheme),
	api_key: str | None = Security(_api_key_header),
	db: Session = Depends(get_db),
) -> models.User | str:
	"""Accept either JWT token or legacy API key. Returns User or 'admin' string."""
	# Try JWT first
	if credentials is not None:
		try:
			payload = decode_access_token(credentials.credentials)
			user_id = payload.get("sub")
			if user_id:
				user = db.query(models.User).filter(models.User.id == int(user_id)).first()
				if user and user.is_active:
					return user
		except HTTPException:
			pass

	# Fall back to API key
	if api_key is not None and hmac.compare_digest(api_key, settings.api_key):
		return "admin"

	raise HTTPException(status_code=HTTP_401_UNAUTHORIZED, detail="Not authenticated")