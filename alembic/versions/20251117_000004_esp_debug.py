"""esp debug logs and debug subscribers

Revision ID: 20251117_000004
Revises: 20251110_000003
Create Date: 2025-11-17 00:00:04
"""

from typing import Sequence, Union

from alembic import op
import sqlalchemy as sa


# revision identifiers, used by Alembic.
revision: str = "20251117_000004"
down_revision: Union[str, None] = "20251110_000003"
branch_labels: Union[str, Sequence[str], None] = None
depends_on: Union[str, Sequence[str], None] = None


def upgrade() -> None:
	op.create_table(
		"esp_debug_logs",
		sa.Column("id", sa.Integer(), primary_key=True),
		sa.Column("station_id", sa.Integer(), nullable=False),
		sa.Column("occurred_at", sa.DateTime(timezone=True), server_default=sa.text("now()"), nullable=False),
		sa.Column("text1", sa.Text(), nullable=True),
		sa.Column("text2", sa.Text(), nullable=True),
		sa.Column("text3", sa.Text(), nullable=True),
		sa.Column("text4", sa.Text(), nullable=True),
		sa.Column("text5", sa.Text(), nullable=True),
		sa.Column("created_at", sa.DateTime(timezone=True), server_default=sa.text("now()"), nullable=False),
		sa.ForeignKeyConstraint(["station_id"], ["stations.id"], ),
	)
	op.create_index("ix_esp_debug_logs_id", "esp_debug_logs", ["id"])

	# Таблица подписчиков отладки
	op.create_table(
		"bot_debug_subscribers",
		sa.Column("id", sa.Integer(), primary_key=True),
		sa.Column("username", sa.String(length=255), nullable=False, unique=True),
		sa.Column("chat_id", sa.BigInteger(), nullable=False),
	)
	op.create_unique_constraint("uq_bot_debug_subscribers_username", "bot_debug_subscribers", ["username"])

	# Notify on insert
	op.execute(
		"""
		CREATE OR REPLACE FUNCTION notify_new_esp_debug() RETURNS trigger AS $$
		BEGIN
			PERFORM pg_notify('esp_debug_channel', row_to_json(NEW)::text);
			RETURN NEW;
		END;
		$$ LANGUAGE plpgsql;
		"""
	)
	op.execute(
		"""
		DO $$
		BEGIN
			IF NOT EXISTS (SELECT 1 FROM pg_trigger WHERE tgname = 'trg_esp_debug_notify') THEN
				CREATE TRIGGER trg_esp_debug_notify
				AFTER INSERT ON esp_debug_logs
				FOR EACH ROW EXECUTE FUNCTION notify_new_esp_debug();
			END IF;
		END $$;
		"""
	)


def downgrade() -> None:
	op.execute(
		"""
		DO $$
		BEGIN
			IF EXISTS (SELECT 1 FROM pg_trigger WHERE tgname = 'trg_esp_debug_notify') THEN
				DROP TRIGGER trg_esp_debug_notify ON esp_debug_logs;
			END IF;
		END $$;
		"""
	)
	op.execute("DROP FUNCTION IF EXISTS notify_new_esp_debug();")
	op.drop_constraint("uq_bot_debug_subscribers_username", "bot_debug_subscribers", type_="unique")
	op.drop_table("bot_debug_subscribers")
	op.drop_index("ix_esp_debug_logs_id", table_name="esp_debug_logs")
	op.drop_table("esp_debug_logs")


