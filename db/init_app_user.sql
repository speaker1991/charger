DO
$$
BEGIN
	IF NOT EXISTS (SELECT FROM pg_roles WHERE rolname = 'app_user') THEN
		CREATE ROLE app_user LOGIN PASSWORD 'change_me_strong';
	END IF;
END
$$;

GRANT CONNECT ON DATABASE charging TO app_user;

REVOKE ALL ON SCHEMA public FROM PUBLIC;
GRANT USAGE ON SCHEMA public TO app_user;

-- На объекты, создаваемые пользователем postgres (дефолтный суперпользователь контейнера),
-- выдаём права по умолчанию для app_user, чтобы миграции Alembic были доступны приложению.
ALTER DEFAULT PRIVILEGES FOR USER postgres IN SCHEMA public
	GRANT SELECT, INSERT, UPDATE, DELETE ON TABLES TO app_user;
ALTER DEFAULT PRIVILEGES FOR USER postgres IN SCHEMA public
	GRANT USAGE, SELECT, UPDATE ON SEQUENCES TO app_user;


