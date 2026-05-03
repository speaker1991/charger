import asyncio
import aiomqtt
import json
import random
import time

STATIONS = [2, 3, 4]

# Начальное состояние станций
state = {
    sid: {
        "charging_active": False,
        "current_power": 0.0,
        "current_kwh": 0.0,
        "pistol": "-",
        "uptime": 0
    }
    for sid in STATIONS
}

async def publish_status(client):
    print("Started publishing loop...")
    while True:
        for sid in STATIONS:
            # Имитация процесса зарядки
            if state[sid]["charging_active"]:
                # Эмуляция колебания напряжения/мощности (например 21-23 кВт)
                state[sid]["current_power"] = round(random.uniform(21.0, 23.0), 2)
                # Добавляем накрученные кВт-ч за прошедшие 3 секунды
                state[sid]["current_kwh"] += (state[sid]["current_power"] / 3600) * 3 
            else:
                state[sid]["current_power"] = 0.0

            state[sid]["uptime"] += 3

            # С вероятностью 2% (каждые ~3 минуты на станцию) имитируем поступление платежа
            if random.random() < 0.02 and not state[sid]["charging_active"]:
                amt = random.choice([300, 500, 1000, 1200])
                paymentMsg = {
                    "transaction_id": f"test_{int(time.time())}_{sid}",
                    "amount": amt,
                    "status": "success",
                    "kwh_purchased": round(amt / 15.0, 1), # Допустим цена 15р
                    "timestamp": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
                }
                print(f"💰 New payment emulated on Station {sid}: {amt} RUB")
                await client.publish(f"stations/{sid}/payments", json.dumps(paymentMsg), retain=True)

            # Формируем статус
            statusMsg = {
                "station_id": sid,
                "charging_active": state[sid]["charging_active"],
                "current_power": state[sid]["current_power"],
                "current_kwh": round(state[sid]["current_kwh"], 2),
                "pistol": state[sid]["pistol"],
                "uptime_seconds": state[sid]["uptime"],
                "free_heap": random.randint(120000, 160000),
                "wifi_rssi": random.randint(-75, -55)
            }
            # Шлём и в status, и в debug (чтобы дашборд подцепил)
            await client.publish(f"stations/{sid}/status", json.dumps(statusMsg))
            await client.publish(f"stations/{sid}/debug", json.dumps(statusMsg))
            
        await asyncio.sleep(3)

async def listen_commands(client):
    async with client.messages() as messages:
        async for message in messages:
            topic = str(message.topic)
            try:
                sid = int(topic.split("/")[1])
                payload = json.loads(message.payload.decode())
                command = payload.get("command")
                print(f"🎮 Station {sid} received command: {command}")
                
                if command == "start_charging":
                    state[sid]["charging_active"] = True
                    state[sid]["pistol"] = payload.get("payload", {}).get("pistol", "A")
                    state[sid]["current_kwh"] = 0.0
                    print(f"  👉 Station {sid} started charging on pistol {state[sid]['pistol']}")
                elif command == "stop_charging":
                    state[sid]["charging_active"] = False
                    state[sid]["pistol"] = "-"
                    print(f"  👉 Station {sid} stopped charging")
                elif command == "restart":
                    state[sid]["uptime"] = 0
                    state[sid]["charging_active"] = False
                    print(f"  👉 Station {sid} restarted")

            except Exception as e:
                print("Error handling message:", e)

async def main():
    print("🔌 Starting ESP32 Emulator...")
    # Подключаемся к брокеру (внутри docker-compose сеть называется mqtt)
    async with aiomqtt.Client("mqtt", 1883) as client:
        print("✅ Emulator connected to MQTT")
        await client.subscribe("stations/+/commands")
        
        task1 = asyncio.create_task(publish_status(client))
        task2 = asyncio.create_task(listen_commands(client))
        
        await asyncio.gather(task1, task2)

if __name__ == "__main__":
    asyncio.run(main())
