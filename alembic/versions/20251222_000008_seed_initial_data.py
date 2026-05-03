"""seed initial address and station data

Revision ID: 20251222_000008
Revises: 20251209_000007
Create Date: 2025-12-22
"""
from typing import Sequence, Union

from alembic import op
import sqlalchemy as sa

import secrets
import os
import hashlib


# revision identifiers, used by Alembic.
revision: str = '20251222_000008'
down_revision: Union[str, None] = '20251209_000007'
branch_labels: Union[str, Sequence[str], None] = None
depends_on: Union[str, Sequence[str], None] = None


def _pbkdf2_sha256_hex(secret: str, salt_hex: str, iterations: int = 200_000) -> str:
    """Возвращает hex-строку PBKDF2-HMAC-SHA256(secret, salt, iterations)."""
    salt = bytes.fromhex(salt_hex)
    dk = hashlib.pbkdf2_hmac("sha256", secret.encode("utf-8"), salt, iterations)
    return dk.hex()


def upgrade() -> None:
    # Генерируем ключ для станции (показывается один раз!)
    plaintext_key = secrets.token_urlsafe(32)
    salt_hex = os.urandom(16).hex()
    hash_hex = _pbkdf2_sha256_hex(plaintext_key, salt_hex)
    fingerprint_hex = hashlib.sha256(plaintext_key.encode("utf-8")).hexdigest()
    
    # Выводим ключ в лог - это единственный момент когда его можно увидеть!
    print("\n" + "=" * 60)
    print("ВАЖНО! Сохраните API-ключ станции (показывается один раз):")
    print(f"Station API Key: {plaintext_key}")
    print("=" * 60 + "\n")
    
    # Вставляем адрес
    op.execute(
        sa.text("""
            INSERT INTO addresses (id, country_code, postal_code, region, city, street, house_number, full_address)
            VALUES (1, 'RU', '658087', 'Алтайский край', 'Новоалтайск', 'Партизанская улица', '6А', 'Партизанская улица, 6А, Новоалтайск, Алтайский край, 658087')
            ON CONFLICT (id) DO NOTHING
        """)
    )
    
    # Обновляем sequence для addresses если используется PostgreSQL
    op.execute(
        sa.text("SELECT setval('addresses_id_seq', GREATEST((SELECT COALESCE(MAX(id), 0) FROM addresses), 1), true)")
    )
    
    # Вставляем станцию
    op.execute(
        sa.text("""
            INSERT INTO stations (id, name, description, latitude, longitude, address_id, api_key_salt, api_key_hash, api_key_fingerprint)
            VALUES (1, 'Livable charging', 'Зарядная станция в Новоалтайске', 53.394324, 83.936089, 1, :salt, :hash, :fingerprint)
            ON CONFLICT (id) DO NOTHING
        """).bindparams(salt=salt_hex, hash=hash_hex, fingerprint=fingerprint_hex)
    )
    
    # Обновляем sequence для stations если используется PostgreSQL
    op.execute(
        sa.text("SELECT setval('stations_id_seq', GREATEST((SELECT COALESCE(MAX(id), 0) FROM stations), 1), true)")
    )


def downgrade() -> None:
    # Удаляем станцию с id=1 (если это именно та, что мы создали)
    op.execute(
        sa.text("DELETE FROM stations WHERE id = 1 AND name = 'Livable charging'")
    )
    
    # Удаляем адрес с id=1
    op.execute(
        sa.text("DELETE FROM addresses WHERE id = 1 AND city = 'Новоалтайск' AND street = 'Партизанская улица'")
    )

