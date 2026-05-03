"""payment transactions

Revision ID: 20251110_000002
Revises: 20251110_000001
Create Date: 2025-11-10 00:50:02
"""

from typing import Sequence, Union

from alembic import op
import sqlalchemy as sa


# revision identifiers, used by Alembic.
revision: str = "20251110_000002"
down_revision: Union[str, None] = "20251110_000001"
branch_labels: Union[str, Sequence[str], None] = None
depends_on: Union[str, Sequence[str], None] = None


def upgrade() -> None:
	op.create_table(
		"payment_transactions",
		sa.Column("id", sa.Integer(), primary_key=True, nullable=False),
		sa.Column("station_id", sa.Integer(), nullable=False),
		sa.Column("occurred_at", sa.TIMESTAMP(timezone=True), server_default=sa.text("now()"), nullable=False),
		sa.Column("amount_paid", sa.Numeric(12, 2), nullable=False),
		sa.Column("refund_amount", sa.Numeric(12, 2), nullable=False, server_default=sa.text("0")),
		sa.Column("kwh_spent", sa.Float(), nullable=False),
		sa.Column("created_at", sa.TIMESTAMP(timezone=True), server_default=sa.text("now()"), nullable=False),
		sa.Column("updated_at", sa.TIMESTAMP(timezone=True), server_default=sa.text("now()"), nullable=False),
		sa.ForeignKeyConstraint(["station_id"], ["stations.id"], name="fk_payment_transactions_station_id_stations"),
	)
	op.create_index("ix_payment_transactions_id", "payment_transactions", ["id"], unique=False)
	op.create_index("ix_payment_transactions_station_id", "payment_transactions", ["station_id"], unique=False)
	op.create_index("ix_payment_transactions_occurred_at", "payment_transactions", ["occurred_at"], unique=False)


def downgrade() -> None:
	op.drop_index("ix_payment_transactions_occurred_at", table_name="payment_transactions")
	op.drop_index("ix_payment_transactions_station_id", table_name="payment_transactions")
	op.drop_index("ix_payment_transactions_id", table_name="payment_transactions")
	op.drop_table("payment_transactions")



