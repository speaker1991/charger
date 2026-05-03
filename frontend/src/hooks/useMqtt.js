import mqtt from 'mqtt';
import { useState, useEffect } from 'react';

// Подключаемся всегда через Caddy proxy рауты /mqtt
// Это решает проблему с закрытыми портами 9001 у провайдера/роутера
// Станции подключаются напрямую к локальному брокеру (мост с 92.53.64.13 decommissioned)
const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
const MQTT_WS_URL = `${protocol}//${window.location.hostname}/mqtt`;
let client = null;

// Попробуем загрузить из localStorage
let savedPayments = [];
try {
  savedPayments = JSON.parse(localStorage.getItem('vincoder_payments')) || [];
} catch (e) {}

const state = {
  stations: {},
  payments: savedPayments,
  connected: false,
};

const listeners = new Set();

function notify() {
  const newState = { ...state, stations: { ...state.stations }, payments: [...state.payments] };
  listeners.forEach(l => l(newState));
}

export function initMqtt() {
  if (client) return;

  // Инициализируем мост в глобальном scope сразу
  window.__VINCODER_MQTT__ = {
    isConnected: () => state.connected,
    ping: (stationId) => {
      if (client) {
        client.publish(`stations/${stationId}/commands`, JSON.stringify({ command: 'ping' }));
        console.log(`Ping sent to ${stationId}`);
      }
    },
    restartStation: (stationId) => {
      if (client) client.publish(`stations/${stationId}/commands`, JSON.stringify({ command: 'restart' }));
    }
  };

  console.log('%c[MQTT] Connecting to: ' + MQTT_WS_URL, 'color: #6366f1; font-weight: bold;');
  client = mqtt.connect(MQTT_WS_URL);

  client.on('connect', () => {
    state.connected = true;
    console.log('%c[MQTT] ✅ Connected successfully to ' + MQTT_WS_URL, 'background: #059669; color: white; padding: 4px 8px; border-radius: 4px; font-weight: bold;');
    client.subscribe('stations/+/status');
    client.subscribe('stations/+/payments');
    client.subscribe('stations/+/response');
    client.subscribe('stations/+/debug');
    console.log('%c[MQTT] 📡 Subscribed to: stations/+/status, +/payments, +/response, +/debug', 'color: #6366f1;');
    notify();
  });

  client.on('error', (err) => {
    console.error('%c[MQTT] ❌ Connection error:', 'background: #dc2626; color: white; padding: 4px 8px; border-radius: 4px; font-weight: bold;', err.message || err);
  });

  client.on('reconnect', () => {
    console.log('%c[MQTT] 🔄 Reconnecting...', 'color: #f59e0b; font-weight: bold;');
  });

  client.on('offline', () => {
    state.connected = false;
    console.log('%c[MQTT] ⚫ Client offline', 'color: #6b7280;');
    notify();
  });

  // Запуск демо-режима и таймауты
  setInterval(() => {
    const now = Date.now();
    let updated = false;

    // Тайм-аут для станции #1 (и всех остальных). Если нет данных > 60 сек, уходит в OFFLINE
    Object.keys(state.stations).forEach(id => {
      const station = state.stations[id];
      if (station && station.isOnline && station.lastSeen && (now - station.lastSeen > 60000)) {
        state.stations[id] = { ...station, isOnline: false };
        updated = true;
      }
    });

    // Генерация фейковых данных ТОЛЬКО для 2, 3 и 4
    ['2', '3', '4'].forEach(id => {
      const station = state.stations[id] || {};
      // Обновляем каждые 2 секунды (убрали 10 сек лимит, чтобы анимация была плавной)
      if (!station.lastSeen || (now - station.lastSeen > 1900)) {
        // У станции 2 (ЭЗС №1) сделаем принудительно зарядку для демонстрации
        // Для остальных - шанс включиться/выключиться
        let isCharging = station.charging_active;
        if (isCharging === undefined) isCharging = (id === '2' || Math.random() > 0.5);
        if (isCharging && station.progress >= 100 && Math.random() > 0.5) isCharging = false;
        if (!isCharging && Math.random() > 0.95) isCharging = true; // 5% chance to start charging if idle
        
        let p = (station.progress || 0);
        let e = (station.eta_minutes || 60);
        if (!isCharging) {
           p = 0;
           e = 60;
        } else {
           p += 2; // +2% per tick
           e = Math.max(0, e - 1);
           if (p > 100) p = 100;
        }

        state.stations[id] = {
          ...station,
          isOnline: true,
          voltage: 220 + Math.floor(Math.random() * 15),
          temp: 30 + Math.floor(Math.random() * 15),
          charging_active: isCharging,
          current_power: isCharging ? parseFloat((Math.random() * 5 + 15).toFixed(2)) : 0,
          current_kwh: isCharging ? (station.current_kwh || 0) + 0.05 : 0,
          progress: p,
          eta_minutes: e,
          uptime: (station.uptime || 3600) + 2,
          free_memory: 120000 + Math.floor(Math.random() * 80000),
          wifi_rssi_terminal: -(40 + Math.floor(Math.random() * 30)),
          wifi_rssi_esp32: -(35 + Math.floor(Math.random() * 25)),
          active_pistol: isCharging ? (station.active_pistol || (Math.random() > 0.5 ? 'A' : 'B')) : null,
          lastSeen: now
        };
        updated = true;
      }
    });
    if (updated) notify();
  }, 2000);

  client.on('message', (topic, message) => {
    try {
      const data = JSON.parse(message.toString());
      if (topic.endsWith('/status')) {
        // station_id из JSON — число, приводим к строке для единообразия с ключами state.stations
        const id = String(data.station_id || topic.split('/')[1]);
        console.log(`%c[MQTT] 📡 Станция #${id}: статус получен (прямое подключение)`, 'background: #047857; color: white; padding: 4px 8px; border-radius: 4px; font-weight: bold;', data);
        state.stations[id] = { ...state.stations[id], ...data, isOnline: true, lastSeen: Date.now() };
        notify();
      } else if (topic.endsWith('/debug')) {
        const id = String(data.station_id || topic.split('/')[1]);
        state.stations[id] = { 
          ...state.stations[id], 
          voltage: data.voltage || data.V, 
          temp: data.temperature || data.temp,
          // Также мержим телеметрические поля если они есть в debug
          ...(data.current_power !== undefined && { current_power: data.current_power }),
          ...(data.current_kwh !== undefined && { current_kwh: data.current_kwh }),
          ...(data.uptime_seconds !== undefined && { uptime_seconds: data.uptime_seconds }),
          ...(data.free_heap !== undefined && { free_heap: data.free_heap }),
          ...(data.wifi_rssi !== undefined && { wifi_rssi: data.wifi_rssi }),
          ...(data.charging_active !== undefined && { charging_active: data.charging_active }),
          lastSeen: Date.now(),
          isOnline: true
        };
        notify();
      } else if (topic.endsWith('/payments')) {
        const amount = data.amount_paid !== undefined ? data.amount_paid : (data.amount || data.sum || 0);
        const kwh = data.kwh_spent !== undefined ? data.kwh_spent : (data.kwh || data.current_kwh || 0);
        const change = data.refund_amount !== undefined ? data.refund_amount : (data.change || 0);

        const newPayment = {
          id: data.transaction_id || data.id || `TX-${Math.floor(Math.random() * 10000)}`,
          type: data.type || 'card',
          user: data.user || 'Оплата картой',
          date: data.date || new Date().toLocaleString('ru-RU', {day: '2-digit', month: '2-digit', year: 'numeric', hour: '2-digit', minute: '2-digit'}).replace(',', ' •'),
          station: `ST-${topic.split('/')[1].padStart(3, '0')}`,
          kwh: kwh,
          sum: amount,
          change: change,
          profit: data.profit || (amount * 0.6), // Грубый расчет прибыли, если нет
          status: data.status || 'success'
        };
        state.payments = [newPayment, ...state.payments].slice(0, 50);
        localStorage.setItem('vincoder_payments', JSON.stringify(state.payments));
        notify();
      }
    } catch (e) {
      console.error('MQTT message error', e);
    }
  });

  client.on('close', () => {
    state.connected = false;
    notify();
  });
}

export function useMqttStore() {
  const [localState, setLocalState] = useState(state);
  
  useEffect(() => {
    initMqtt();
    const listener = (s) => setLocalState(s);
    listeners.add(listener);
    return () => listeners.delete(listener);
  }, []);

  return localState;
}

// React хук по инструкции
export function useMqttStation(stationId) {
  const { stations, connected } = useMqttStore();
  const station = stations[stationId] || {};

  // Map real hardware values or fallback to simulation format
  const isCharging = (station.port_a?.charging_status === 1) || (station.port_b?.charging_status === 1) || !!station.charging_active;
  const activePistol = station.port_a?.charging_status === 1 ? 'A' : (station.port_b?.charging_status === 1 ? 'B' : station.active_pistol);
  const meterError = station.port_a?.meter_error || station.port_b?.meter_error || !!station.meter_error;
  const currentKwh = station.port_a?.kwh_available || station.port_b?.kwh_available || station.current_kwh || 0;

  return {
    isOnline: !!station.isOnline,
    chargingActive: isCharging,
    currentPower: station.current_power || 0,
    currentKwh: currentKwh,
    meterError: meterError,
    voltage: station.voltage || '-',
    temp: station.temp || '-',
    pistol: station.pistol || '-',
    activePistol: activePistol || null,
    progress: station.progress || 0,
    etaMinutes: station.eta_minutes || 0,
    uptime: station.uptime_seconds !== undefined ? station.uptime_seconds : (station.uptime || 0),
    freeMemory: station.free_heap !== undefined ? station.free_heap : (station.free_memory || 0),
    wifiRssi: station.wifi_rssi || 0,
    wifiRssiTerminal: station.wifi_rssi_terminal || 0,
    wifiRssiEsp32: station.wifi_rssi !== undefined ? station.wifi_rssi : (station.wifi_rssi_esp32 || 0),
    mqttConnected: connected,
    freeCharging: (pistol) => {
      console.log(`Sending Free Charging to ${stationId} on ${pistol}`, { client: !!client });
      client?.publish(`stations/${stationId}/commands`, JSON.stringify({ command: `free_charging_${pistol.toLowerCase()}` }));
    },
    limitCurrent: (max_current) => {
      console.log(`Sending Limit Current ${max_current}A to ${stationId}`, { client: !!client });
      client?.publish(`stations/${stationId}/commands`, JSON.stringify({ command: 'set_max_current', payload: { max_current } }));
    },
    restart: () => {
      console.log(`Sending Restart to ${stationId}`, { client: !!client });
      client?.publish(`stations/${stationId}/commands`, JSON.stringify({ command: 'restart' }));
    },
  };
}
