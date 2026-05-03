"""add pistol field to payment_transactions

Revision ID: 20251118_000006
Revises: 20251118_000005
Create Date: 2025-11-18 00:00:06
"""

from typing import Sequence, Union

from alembic import op
import sqlalchemy as sa


# revision identifiers, used by Alembic.
revision: str = "20251118_000006"
down_revision: Union[str, None] = "20251118_000005"
branch_labels: Union[str, Sequence[str], None] = None
depends_on: Union[str, Sequence[str], None] = None


def upgrade() -> None:
	op.add_column("payment_transactions", sa.Column("pistol", sa.Text(), nullable=True))


def downgrade() -> None:
	op.drop_column("payment_transactions", "pistol")


