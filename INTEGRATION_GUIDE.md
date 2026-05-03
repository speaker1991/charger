# Vincoder MQTT WebSocket Integration Guide

> Полная инструкция по подключению React-дашборда к реальным зарядным станциям
> через Mosquitto WebSocket — без единого изменения в Python-бэкенде или C++ прошивке.

---

## Содержание

1. [Архитектура решения](#1-архитектура-решения)
2. [Карта MQTT-протокола](#2-карта-mqtt-протокола)
3. [Инструкция по установке](#3-инструкция-по-установке)
4. [Как это работает внутри](#4-как-это-работает-внутри)
5. [React-хук для глубокой интеграции](#5-react-хук-для-глубокой-интеграции)
6. [Консольный API](#6-консольный-api)
7. [Чеклист деплоя](#7-чеклист-деплоя)
8. [Безопасность](#8-безопасность)

---

## 1. Архитектура решения

```
                     ┌─────────────┐
                     │  ESP32 #1   │ ──── MQTT 1883 ────┐
                     │ (Станция 1) │                    │
                     └─────────────┘                    ▼
                                               ┌──────────────┐
┌─────────────┐                                │  MOSQUITTO   │
│  ESP32 #2   │ ──── MQTT 1883 ───────────────▶│  MQTT Broker │
│ (Станция 2) │                                │              │
└─────────────┘                                │ :1883 (TCP)  │
                                               │ :9001 (WS) ◀─── ЭТО НАША ТОЧКА ВХОДА
┌─────────────┐                                └──────┬───────┘
│  ESP32 #N   │ ──── MQTT 1883 ────────────────────┘  │
└─────────────┘                                       │ WebSocket :9001
                                                      │
                                               ┌──────▼───────┐
                                               │   БРАУЗЕР     │
                                               │               │
                                               │  React SPA    │
                                               │  + mqtt.js    │
                                               │  + Bridge.js  │
                                               └───────────────┘

   Python API + Telegram Bot  →  НИКАК НЕ ЗАТРОНУТЫ (даже не знают о нас)
```

**Ключевая идея**: Mosquitto уже имеет WebSocket listener на порту 9001 (см. `mosquitto.conf` строка 13-14, `docker-compose.yml` строка 27). Анонимный доступ разрешён (`allow_anonymous true`). Дашборд подключается к брокеру напрямую через `ws://SERVER:9001`, подписывается на топики станций и отправляет команды — всё в браузере.

---

## 2. Карта MQTT-протокола

### 2.1. Топики

| Направление | Топик | QoS | Описание |
|---|---|---|---|
| **ESP32 → Брокер** | `stations/{id}/status` | 0 | Heartbeat каждые 30 сек |
| **ESP32 → Брокер** | `stations/{id}/response` | 1 | Ответ на команду |
| **ESP32 → Брокер** | `stations/{id}/debug` | 0 | Debug-логи (text1..text6) |
| **ESP32 → Брокер** | `stations/{id}/payments` | 0 | Транзакции платежей |
| **Дашборд → ESP32** | `stations/{id}/commands` | 1 | Команды управления |

> `{id}` — числовой ID станции (1, 2, 3, 4...)

### 2.2. Маппинг ID

| Дашборд | MQTT station_id | Название |
|---|---|---|
| `ST-MAIN` | 1 | Главная (Партизанская 6А) |
| `ST-002` | 2 | ТЦ Каскад |
| `ST-003` | 3 | Вокзальная |
| `ST-004` | 4 | Белоярск |

### 2.3. JSON: Статус от ESP32

```json
{
  "station_id": 1,
  "charging_active": true,
  "current_power": 3.75,
  "current_kwh": 4.52,
  "pistol": "B",
  "uptime_seconds": 86400,
  "free_heap": 245760,
  "wifi_rssi": -65
}
```

| Поле | Тип | Описание |
|---|---|---|
| `station_id` | int | ID станции |
| `charging_active` | bool | Идёт ли зарядка |
| `current_power` | float | Текущая мощность, кВт |
| `current_kwh` | float | Отданная энергия (текущая сессия), кВт·ч |
| `pistol` | string | Активный пистолет ("A", "B", "") |
| `uptime_seconds` | int | Время работы ESP32, сек |
| `free_heap` | int | Свободная RAM, байт |
| `wifi_rssi` | int | Уровень WiFi сигнала, dBm |

### 2.4. JSON: Команда на ESP32

```json
{
  "command": "get_status",
  "command_id": "550e8400-e29b-41d4-a716-446655440000",
  "timestamp": "2026-04-13T12:00:00.000Z",
  "payload": {}
}
```

**Доступные команды:**

| command | payload | Действие |
|---|---|---|
| `get_status` | — | Запросить полный статус |
| `stop_charging` | — | Остановить зарядку |
| `start_charging` | `{ "pistol": "A" }` | Запустить зарядку |
| `restart` | — | Перезагрузить ESP32 |
| `ping` | — | Проверка связи (pong) |
| `set_max_current` | `{ "max_current": 32.0 }` | Установить макс. ток (А) |

### 2.5. JSON: Ответ от ESP32

```json
{
  "command_id": "550e8400-e29b-41d4-...",
  "status": "executed",
  "response": {
    "message": "Charging stopped",
    "charging_active": false,
    "current_power": 0
  },
  "timestamp": 86400000
}
```

`status`: `"executed"` — успех, `"failed"` — ошибка.

---

## 3. Инструкция по установке

### Шаг 1: CDN-зависимость

В `<head>` вашего `index.html` добавьте **перед** скриптом бандла:

```html
<script src="https://unpkg.com/mqtt@5.10.1/dist/mqtt.min.js"></script>
```

### Шаг 2: Скрипт моста

Перед `</body>` (или подключите как отдельный файл) добавьте:

```html
<script src="vincoder-mqtt-bridge.js"></script>
```

### Шаг 3: Настройка

Откройте `vincoder-mqtt-bridge.js` и замените одну строку:

```js
// БЫЛО:
const MQTT_WS_URL = 'ws://YOUR_SERVER_IP:9001';

// СТАЛО (пример):
const MQTT_WS_URL = 'ws://185.123.45.67:9001';
```

Если у вас домен с TLS — используйте `wss://`:

```js
const MQTT_WS_URL = 'wss://mqtt.vincoder.io:9001';
```

### Шаг 4: Проверка

1. Откройте дашборд в браузере
2. Нажмите `Ctrl+Shift+M` — появится debug-панель
3. Должно быть: `✅ MQTT подключен к брокеру`
4. Нажмите "Ping MAIN" — должен прийти ответ

---

## 4. Как это работает внутри

### Цикл данных (каждые 30 сек):

```
ESP32  ──publish──▶  stations/1/status  ──▶  Mosquitto  ──ws──▶  Browser
                                                                    │
                                                            handleStatus()
                                                                    │
                                                              patchDOM()
                                                                    │
                                                     Обновление значений:
                                                     • Мощность → 3.75 кВт
                                                     • Ток → 16.2 А
                                                     • Статус порта → Заряжается
                                                     • Индикатор → 🟢 ONLINE
```

### Цикл команд (по нажатию кнопки):

```
User clicks "Reboot"
       │
       ▼
sendCommand('ST-MAIN', 'restart')
       │
       ▼
publish ──▶ stations/1/commands ──▶ Mosquitto ──1883──▶ ESP32
                                                         │
                                                    ESP.restart()
                                                         │
                                                     response
                                                         │
ESP32 ──publish──▶ stations/1/response ──▶ Mosquitto ──ws──▶ Browser
                                                              │
                                                     Promise resolves
                                                     Кнопка → "✅ Done"
```

### DOM-патчинг:

Мост находит элементы UI по характерным текстам (например, "Мощность", "Ток", "WiFi / MQTT") и подменяет значения. React не перезатирает эти изменения, потому что они вне его virtual DOM до следующего re-render.

Привязка к кнопкам (Reboot, Update) происходит через `MutationObserver` + capture-phase event listener, который перехватывает клик до React-обработчика.

---

## 5. React-хук для глубокой интеграции

Если вы пересобираете React-компоненты (имеете доступ к исходникам), используйте хук `useMqttStation`:

```jsx
import { useMqttStation } from './useMqttStation';

function StationMonitor({ stationId }) {
  const {
    isOnline,
    chargingActive,
    currentPower,
    currentKwh,
    pistol,
    wifiRssi,
    mqttConnected,
    stopCharging,
    startCharging,
    restart,
    ping,
    getStatus,
    setMaxCurrent,
  } = useMqttStation(stationId); // 'ST-MAIN'

  return (
    <div className="p-6 rounded-2xl bg-white shadow-lg">
      {/* Индикатор связи */}
      <div className="flex items-center gap-2 mb-4">
        <div className={`w-2.5 h-2.5 rounded-full animate-pulse ${
          isOnline ? 'bg-emerald-500' : 'bg-rose-500'
        }`} />
        <span className="text-xs font-bold text-slate-500 uppercase tracking-widest">
          {isOnline ? 'Online' : 'Offline'}
        </span>
        {!mqttConnected && (
          <span className="text-xs text-amber-500">MQTT disconnected</span>
        )}
      </div>

      {/* Телеметрия */}
      <div className="grid grid-cols-2 gap-4 mb-6">
        <div className="p-4 bg-slate-50 rounded-2xl">
          <p className="text-[9px] text-slate-400 font-black uppercase">Мощность</p>
          <p className="text-xl font-black text-teal-950">
            {currentPower.toFixed(2)} кВт
          </p>
        </div>
        <div className="p-4 bg-slate-50 rounded-2xl">
          <p className="text-[9px] text-slate-400 font-black uppercase">Энергия</p>
          <p className="text-xl font-black text-teal-950">
            {currentKwh.toFixed(2)} кВт·ч
          </p>
        </div>
        <div className="p-4 bg-slate-50 rounded-2xl">
          <p className="text-[9px] text-slate-400 font-black uppercase">Пистолет</p>
          <p className="text-xl font-black text-teal-950">
            {chargingActive ? `${pistol} ⚡` : '—'}
          </p>
        </div>
        <div className="p-4 bg-slate-50 rounded-2xl">
          <p className="text-[9px] text-slate-400 font-black uppercase">WiFi</p>
          <p className="text-xl font-black text-teal-950">{wifiRssi} dBm</p>
        </div>
      </div>

      {/* Управление */}
      <div className="grid grid-cols-2 gap-2">
        {chargingActive ? (
          <button
            onClick={stopCharging}
            className="p-4 bg-rose-50 text-rose-600 rounded-2xl text-xs font-black uppercase hover:bg-rose-100 active:scale-95 transition-all"
          >
            Стоп зарядка
          </button>
        ) : (
          <button
            onClick={() => startCharging('A')}
            className="p-4 bg-emerald-50 text-emerald-600 rounded-2xl text-xs font-black uppercase hover:bg-emerald-100 active:scale-95 transition-all"
          >
            Старт (A)
          </button>
        )}
        <button
          onClick={restart}
          className="p-4 bg-rose-50 text-rose-600 rounded-2xl text-xs font-black uppercase hover:bg-rose-100 active:scale-95 transition-all"
        >
          Reboot
        </button>
      </div>
    </div>
  );
}
```

### Хук для всех станций разом:

```jsx
import { useMqttAllStations } from './useMqttStation';

function NetworkOverview() {
  const { states, mqttConnected } = useMqttAllStations();

  const totalPower = Object.values(states)
    .reduce((sum, s) => sum + (s.currentPower || 0), 0);

  const onlineCount = Object.values(states)
    .filter(s => s.isOnline).length;

  return (
    <div>
      <p>Онлайн: {onlineCount} / {Object.keys(states).length}</p>
      <p>Суммарная мощность: {totalPower.toFixed(1)} кВт</p>
    </div>
  );
}
```

---

## 6. Консольный API

После загрузки страницы в DevTools Console (`F12`):

```js
// Подключение
__VINCODER_MQTT__.isConnected()        // → true/false

// Пинг (проверка связи)
await __VINCODER_MQTT__.ping('ST-MAIN')
// → { command_id: "...", status: "executed", response: { message: "pong" } }

// Полный статус
await __VINCODER_MQTT__.getStatus('ST-MAIN')

// Управление зарядкой
await __VINCODER_MQTT__.startCharging('ST-MAIN', 'A')
await __VINCODER_MQTT__.stopCharging('ST-MAIN')

// Перезагрузка
await __VINCODER_MQTT__.restartStation('ST-MAIN')

// Максимальный ток
await __VINCODER_MQTT__.setMaxCurrent('ST-MAIN', 16.0)

// Кэш состояний
__VINCODER_MQTT__.getStationState('ST-MAIN')
__VINCODER_MQTT__.getAllStates()

// Подписка на обновления
__VINCODER_MQTT__.on('onStatusUpdate', (id, data) => {
  console.table({ station: id, power: data.currentPower, kwh: data.currentKwh });
});
```

---

## 7. Чеклист деплоя

- [ ] Замени `YOUR_SERVER_IP` в `MQTT_WS_URL`
- [ ] Порт `9001` открыт в firewall / Security Group
- [ ] CDN-скрипт mqtt.js добавлен в `<head>`
- [ ] `vincoder-mqtt-bridge.js` подключен перед `</body>`
- [ ] Маппинг `STATION_MAP` совпадает с реальными ID в БД
- [ ] `Ctrl+Shift+M` показывает debug-панель
- [ ] Статус: `✅ MQTT подключен`
- [ ] Ping MAIN возвращает `pong`
- [ ] Реальные кВт обновляются в блоке «4. Питание»
- [ ] Кнопка Reboot отправляет команду и показывает результат

---

## 8. Безопасность

Текущая конфигурация: `allow_anonymous true` — подходит для разработки, но **не для продакшена**.

### Минимальная защита:

1. **Mosquitto аутентификация:**
   ```bash
   # Создать пользователя
   docker exec -it <mqtt_container> mosquitto_passwd -c /mosquitto/config/passwd dashboard
   
   # В mosquitto.conf:
   allow_anonymous false
   password_file /mosquitto/config/passwd
   ```

2. **В Bridge добавить credentials:**
   ```js
   client = mqtt.connect(MQTT_WS_URL, {
     username: 'dashboard',
     password: 'YourSecurePassword',
     // ...
   });
   ```

3. **TLS (WSS)** — через nginx reverse proxy:
   ```nginx
   server {
     listen 443 ssl;
     server_name mqtt.vincoder.io;
     
     ssl_certificate     /path/to/cert.pem;
     ssl_certificate_key /path/to/key.pem;
     
     location / {
       proxy_pass http://localhost:9001;
       proxy_http_version 1.1;
       proxy_set_header Upgrade $http_upgrade;
       proxy_set_header Connection "upgrade";
     }
   }
   ```

4. **ACL (Access Control List)** — ограничить дашборд только на чтение:
   ```
   # /mosquitto/config/acl
   user dashboard
   topic read stations/+/status
   topic read stations/+/response
   topic read stations/+/debug
   topic write stations/+/commands
   ```
