"""station_commands table for MQTT commands

Revision ID: 20251209_000007
Revises: 20251118_000006
Create Date: 2025-12-09

"""
from typing import Sequence, Union

from alembic import op
import sqlalchemy as sa


# revision identifiers, used by Alembic.
revision: str = '20251209_000007'
down_revision: Union[str, None] = '20251118_000006'
branch_labels: Union[str, Sequence[str], None] = None
depends_on: Union[str, Sequence[str], None] = None


def upgrade() -> None:
    op.create_table(
        'station_commands',
        sa.Column('id', sa.Integer(), nullable=False),
        sa.Column('command_id', sa.String(36), nullable=False),
        sa.Column('station_id', sa.Integer(), nullable=False),
        sa.Column('command_type', sa.String(50), nullable=False),
        sa.Column('payload', sa.Text(), nullable=True),
        sa.Column('status', sa.String(20), nullable=False, server_default='pending'),
        sa.Column('sent_by', sa.String(100), nullable=True),
        sa.Column('response', sa.Text(), nullable=True),
        sa.Column('created_at', sa.DateTime(timezone=True), server_default=sa.func.now(), nullable=False),
        sa.Column('sent_at', sa.DateTime(timezone=True), nullable=True),
        sa.Column('responded_at', sa.DateTime(timezone=True), nullable=True),
        sa.PrimaryKeyConstraint('id'),
        sa.ForeignKeyConstraint(['station_id'], ['stations.id'], ondelete='CASCADE'),
    )
    op.create_index('ix_station_commands_id', 'station_commands', ['id'])
    op.create_index('ix_station_commands_command_id', 'station_commands', ['command_id'], unique=True)
    op.create_index('ix_station_commands_station_id', 'station_commands', ['station_id'])
    op.create_index('ix_station_commands_command_type', 'station_commands', ['command_type'])
    op.create_index('ix_station_commands_status', 'station_commands', ['status'])


def downgrade() -> None:
    op.drop_index('ix_station_commands_status')
    op.drop_index('ix_station_commands_command_type')
    op.drop_index('ix_station_commands_station_id')
    op.drop_index('ix_station_commands_command_id')
    op.drop_index('ix_station_commands_id')
    op.drop_table('station_commands')

