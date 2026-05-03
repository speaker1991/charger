"""add text6 to esp_debug_logs

Revision ID: 20251118_000005
Revises: 20251117_000004
Create Date: 2025-11-18 00:00:05
"""

from typing import Sequence, Union

from alembic import op
import sqlalchemy as sa


# revision identifiers, used by Alembic.
revision: str = "20251118_000005"
down_revision: Union[str, None] = "20251117_000004"
branch_labels: Union[str, Sequence[str], None] = None
depends_on: Union[str, Sequence[str], None] = None


def upgrade() -> None:
	op.add_column("esp_debug_logs", sa.Column("text6", sa.Text(), nullable=True))


def downgrade() -> None:
	op.drop_column("esp_debug_logs", "text6")


