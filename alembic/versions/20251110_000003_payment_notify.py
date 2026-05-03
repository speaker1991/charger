"""payment transactions notify trigger

Revision ID: 20251110_000003
Revises: 20251110_000002
Create Date: 2025-11-10 01:00:03
"""

from typing import Sequence, Union

from alembic import op


# revision identifiers, used by Alembic.
revision: str = "20251110_000003"
down_revision: Union[str, None] = "20251110_000002"
branch_labels: Union[str, Sequence[str], None] = None
depends_on: Union[str, Sequence[str], None] = None


def upgrade() -> None:
	op.execute(
		"""
		CREATE OR REPLACE FUNCTION notify_new_payment_transaction() RETURNS trigger AS $$
		BEGIN
			PERFORM pg_notify('payment_transactions_channel', row_to_json(NEW)::text);
			RETURN NEW;
		END;
		$$ LANGUAGE plpgsql;
		"""
	)
	op.execute(
		"""
		DO $$
		BEGIN
			IF NOT EXISTS (SELECT 1 FROM pg_trigger WHERE tgname = 'trg_payment_transactions_notify') THEN
				CREATE TRIGGER trg_payment_transactions_notify
				AFTER INSERT ON payment_transactions
				FOR EACH ROW EXECUTE FUNCTION notify_new_payment_transaction();
			END IF;
		END $$;
		"""
	)


def downgrade() -> None:
	op.execute(
		"""
		DO $$
		BEGIN
			IF EXISTS (SELECT 1 FROM pg_trigger WHERE tgname = 'trg_payment_transactions_notify') THEN
				DROP TRIGGER trg_payment_transactions_notify ON payment_transactions;
			END IF;
		END $$;
		"""
	)
	op.execute("DROP FUNCTION IF EXISTS notify_new_payment_transaction();")



