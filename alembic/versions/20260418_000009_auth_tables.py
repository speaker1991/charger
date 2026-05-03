"""
Migration: Add auth tables (users, invites, user_station_access, free_charging_sessions).
Seed initial super_admin user.

Revision ID: 20260418_000009
Revises: 20251222_000008
"""
from alembic import op
import sqlalchemy as sa
from datetime import datetime, timezone

# revision identifiers
revision = "20260418_000009"
down_revision = "20251222_000008"
branch_labels = None
depends_on = None


def upgrade() -> None:
    # Users table
    op.create_table(
        "users",
        sa.Column("id", sa.Integer(), primary_key=True, autoincrement=True),
        sa.Column("email", sa.String(255), nullable=False, unique=True, index=True),
        sa.Column("password_hash", sa.String(255), nullable=False),
        sa.Column("full_name", sa.String(255), nullable=False),
        sa.Column("phone", sa.String(50), nullable=True),
        sa.Column("role", sa.String(20), nullable=False, server_default="employee", index=True),
        sa.Column("owner_id", sa.Integer(), sa.ForeignKey("users.id"), nullable=True),
        sa.Column("is_active", sa.Integer(), nullable=False, server_default="1"),
        sa.Column("created_at", sa.DateTime(timezone=True), server_default=sa.func.now(), nullable=False),
        sa.Column("updated_at", sa.DateTime(timezone=True), server_default=sa.func.now(), nullable=False),
    )

    # Invites table
    op.create_table(
        "invites",
        sa.Column("id", sa.Integer(), primary_key=True, autoincrement=True),
        sa.Column("token", sa.String(64), nullable=False, unique=True, index=True),
        sa.Column("target_role", sa.String(20), nullable=False),
        sa.Column("station_ids", sa.Text(), nullable=True),
        sa.Column("created_by", sa.Integer(), sa.ForeignKey("users.id"), nullable=False),
        sa.Column("is_used", sa.Integer(), nullable=False, server_default="0"),
        sa.Column("used_by", sa.Integer(), sa.ForeignKey("users.id"), nullable=True),
        sa.Column("created_at", sa.DateTime(timezone=True), server_default=sa.func.now(), nullable=False),
    )

    # User-Station access mapping
    op.create_table(
        "user_station_access",
        sa.Column("id", sa.Integer(), primary_key=True, autoincrement=True),
        sa.Column("user_id", sa.Integer(), sa.ForeignKey("users.id"), nullable=False),
        sa.Column("station_id", sa.Integer(), sa.ForeignKey("stations.id"), nullable=False),
    )
    op.create_index("ix_user_station_access_user_id", "user_station_access", ["user_id"])
    op.create_index("ix_user_station_access_station_id", "user_station_access", ["station_id"])

    # Free charging sessions
    op.create_table(
        "free_charging_sessions",
        sa.Column("id", sa.Integer(), primary_key=True, autoincrement=True),
        sa.Column("user_id", sa.Integer(), sa.ForeignKey("users.id"), nullable=False),
        sa.Column("station_id", sa.Integer(), sa.ForeignKey("stations.id"), nullable=False),
        sa.Column("started_at", sa.DateTime(timezone=True), server_default=sa.func.now(), nullable=False),
        sa.Column("kwh_spent", sa.Float(), nullable=False, server_default="0"),
        sa.Column("is_active", sa.Integer(), nullable=False, server_default="1"),
        sa.Column("finished_at", sa.DateTime(timezone=True), nullable=True),
    )
    op.create_index("ix_free_charging_sessions_user_id", "free_charging_sessions", ["user_id"])
    op.create_index("ix_free_charging_sessions_station_id", "free_charging_sessions", ["station_id"])

    # Seed super_admin user (password: admin2026)
    from passlib.context import CryptContext
    pwd = CryptContext(schemes=["bcrypt"], deprecated="auto")
    hashed = pwd.hash("admin2026")

    op.execute(
        sa.text(
            "INSERT INTO users (email, password_hash, full_name, role, is_active, created_at, updated_at) "
            "VALUES (:email, :pwd, :name, :role, 1, NOW(), NOW())"
        ).bindparams(
            email="admin@vincoder.com",
            pwd=hashed,
            name="Super Admin",
            role="super_admin",
        )
    )


def downgrade() -> None:
    op.drop_table("free_charging_sessions")
    op.drop_table("user_station_access")
    op.drop_table("invites")
    op.drop_table("users")
