FROM python:3.11-slim

ENV PYTHONDONTWRITEBYTECODE=1 \
	PYTHONUNBUFFERED=1

WORKDIR /app

COPY requirements.txt ./
RUN pip install --no-cache-dir -r requirements.txt

COPY app ./app
COPY alembic.ini ./alembic.ini
COPY alembic ./alembic
COPY docker-entrypoint.sh ./docker-entrypoint.sh
# Нормализация переводов строк CRLF→LF (устраняет env: 'sh\r' и подобные ошибки)
RUN find . -type f -exec sed -i 's/\r$//' {} +
RUN chmod +x ./docker-entrypoint.sh

EXPOSE 8000

CMD ["./docker-entrypoint.sh"]
