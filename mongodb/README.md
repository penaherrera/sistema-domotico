# MongoDB · domotica_db

| Colección | Quién escribe | Contenido |
|---|---|---|
| `eventos` | Node-RED | `alerta_gas`, `fuga_agua`, `puerta_abierta` (del ESP32), `error_esp32`, `lectura_invalida`, `orden_dashboard` (auditoría de cada clic), `esp32_online` / `esp32_offline` (LWT) |
| `estado_actuadores` | Node-RED (upsert) | `rele`, `buzzer`, `ventilador` (`estado`), `servo` (`angulo`), `sistema` (`modo`, `seguridad`) |

Ejemplo de evento:

```json
{ "tipo": "alerta_gas", "sensor": "MQ2", "valor": 960, "umbral": 945,
  "actuadores": ["buzzer", "ventilador"], "modo": "auto", "origen": "esp32",
  "fecha": ISODate("2026-11-25T21:40:00Z") }
```

Configuración del servicio: [`../servidor/mongodb/mongod.conf`](../servidor/mongodb/mongod.conf).
