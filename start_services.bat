@echo off
echo [1/3] Ожидание Docker daemon...

:wait_loop
docker info >nul 2>&1
if %ERRORLEVEL% == 0 goto docker_ready
echo Docker ещё не готов, ждём 5 секунд...
timeout /t 5 /nobreak >nul
goto wait_loop

:docker_ready
echo [2/3] Docker daemon готов!

echo [3/3] Запуск сервисов (db, mqtt, api, bot)...
docker-compose --profile bot up -d --build

if %ERRORLEVEL% == 0 (
    echo.
    echo === Все сервисы запущены! ===
    echo.
    docker-compose ps
    echo.
    echo Порты:
    echo   API        -^> http://localhost:8000
    echo   MQTT       -^> mqtt://localhost:1883
    echo   MQTT WS    -^> ws://localhost:9001  [Dashboard]
) else (
    echo ОШИБКА: docker-compose завершился с ошибкой!
    exit /b 1
)
