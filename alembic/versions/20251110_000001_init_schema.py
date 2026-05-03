"""initial schema (squashed)

Revision ID: 20251110_000001
Revises: 
Create Date: 2025-11-10 00:35:01
"""

from typing import Sequence, Union

from alembic import op
import sqlalchemy as sa


# revision identifiers, used by Alembic.
revision: str = "20251110_000001"
down_revision: Union[str, None] = None
branch_labels: Union[str, Sequence[str], None] = None
depends_on: Union[str, Sequence[str], None] = None


def upgrade() -> None:
	# addresses
	op.create_table(
		"addresses",
		sa.Column("id", sa.Integer(), primary_key=True, nullable=False),
		sa.Column("country_code", sa.String(length=2), nullable=False),
		sa.Column("postal_code", sa.String(length=20), nullable=True),
		sa.Column("region", sa.String(length=100), nullable=True),
		sa.Column("district", sa.String(length=100), nullable=True),
		sa.Column("city", sa.String(length=100), nullable=False),
		sa.Column("street", sa.String(length=200), nullable=True),
		sa.Column("house_number", sa.String(length=20), nullable=True),
		sa.Column("building", sa.String(length=20), nullable=True),
		sa.Column("apartment", sa.String(length=20), nullable=True),
		sa.Column("full_address", sa.Text(), nullable=True),
	)
	op.create_index("ix_addresses_id", "addresses", ["id"], unique=False)
	op.create_index("ix_addresses_city", "addresses", ["city"], unique=False)

	# stations
	op.create_table(
		"stations",
		sa.Column("id", sa.Integer(), primary_key=True, nullable=False),
		sa.Column("name", sa.String(length=255), nullable=False),
		sa.Column("description", sa.Text(), nullable=True),
		sa.Column("latitude", sa.Float(), nullable=True),
		sa.Column("longitude", sa.Float(), nullable=True),
		sa.Column("address_id", sa.Integer(), nullable=False),
		sa.Column("api_key_salt", sa.String(length=64), nullable=False),
		sa.Column("api_key_hash", sa.String(length=128), nullable=False),
		sa.Column("api_key_fingerprint", sa.String(length=64), nullable=True),
		sa.Column("created_at", sa.TIMESTAMP(timezone=True), server_default=sa.text("now()"), nullable=False),
		sa.Column("updated_at", sa.TIMESTAMP(timezone=True), server_default=sa.text("now()"), nullable=False),
		sa.ForeignKeyConstraint(["address_id"], ["addresses.id"], name="fk_stations_address_id_addresses"),
	)
	op.create_index("ix_stations_id", "stations", ["id"], unique=False)
	op.create_index("ix_stations_name", "stations", ["name"], unique=False)
	op.create_index("ix_stations_api_key_hash", "stations", ["api_key_hash"], unique=False)
	op.create_index("ix_stations_api_key_fingerprint", "stations", ["api_key_fingerprint"], unique=False)

	# measurements
	op.create_table(
		"measurements",
		sa.Column("id", sa.Integer(), primary_key=True, nullable=False),
		sa.Column("current", sa.Float(), nullable=False),
		sa.Column("voltage", sa.Float(), nullable=False),
		sa.Column("station_id", sa.Integer(), nullable=False),
		sa.Column("created_at", sa.TIMESTAMP(timezone=True), server_default=sa.text("now()"), nullable=False),
		sa.Column("updated_at", sa.TIMESTAMP(timezone=True), server_default=sa.text("now()"), nullable=False),
		sa.ForeignKeyConstraint(["station_id"], ["stations.id"], name="fk_measurements_station_id_stations"),
	)
	op.create_index("ix_measurements_id", "measurements", ["id"], unique=False)
	op.create_index("ix_measurements_station_id", "measurements", ["station_id"], unique=False)

	# notify function and trigger for inserts (used by bot)
	op.execute(
		"""
		CREATE OR REPLACE FUNCTION notify_new_measurement() RETURNS trigger AS $$
		BEGIN
			PERFORM pg_notify('measurements_channel', row_to_json(NEW)::text);
			RETURN NEW;
		END;
		$$ LANGUAGE plpgsql;
		"""
	)
	op.execute(
		"""
		DO $$
		BEGIN
			IF NOT EXISTS (SELECT 1 FROM pg_trigger WHERE tgname = 'trg_measurements_notify') THEN
				CREATE TRIGGER trg_measurements_notify
				AFTER INSERT ON measurements
				FOR EACH ROW EXECUTE FUNCTION notify_new_measurement();
			END IF;
		END $$;
		"""
	)

	# bot_subscribers (для уведомлений)
	op.create_table(
		"bot_subscribers",
		sa.Column("id", sa.Integer(), primary_key=True, nullable=False),
		sa.Column("username", sa.Text(), nullable=False),
		sa.Column("chat_id", sa.BigInteger(), nullable=False),
		sa.Column("created_at", sa.TIMESTAMP(timezone=True), server_default=sa.text("now()"), nullable=False),
	)
	op.create_unique_constraint("uq_bot_subscribers_username", "bot_subscribers", ["username"])


def downgrade() -> None:
	# bot_subscribers
	op.drop_constraint("uq_bot_subscribers_username", "bot_subscribers", type_="unique")
	op.drop_table("bot_subscribers")

	# measurements trigger and function
	op.execute(
		"""
		DO $$
		BEGIN
			IF EXISTS (SELECT 1 FROM pg_trigger WHERE tgname = 'trg_measurements_notify') THEN
				DROP TRIGGER trg_measurements_notify ON measurements;
			END IF;
		END $$;
		"""
	)
	op.execute("DROP FUNCTION IF EXISTS notify_new_measurement();")

	# measurements
	op.drop_index("ix_measurements_station_id", table_name="measurements")
	op.drop_index("ix_measurements_id", table_name="measurements")
	op.drop_table("measurements")

	# stations
	op.drop_index("ix_stations_api_key_fingerprint", table_name="stations")
	op.drop_index("ix_stations_api_key_hash", table_name="stations")
	op.drop_index("ix_stations_name", table_name="stations")
	op.drop_index("ix_stations_id", table_name="stations")
	op.drop_table("stations")

	# addresses
	op.drop_index("ix_addresses_city", table_name="addresses")
	op.drop_index("ix_addresses_id", table_name="addresses")
	op.drop_table("addresses")



