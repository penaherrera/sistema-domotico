# Node-RED 5.0.7 + Dashboard 2.0

`flows.json` es el flujo completo de la Fase 4 (77 nodos, 3 pestañas). **No contiene contraseñas**: Node-RED las guarda cifradas en `flows_cred.json`, que está excluido del repositorio.

## Nodos necesarios

```bash
cd /srv/nodered
npm install @flowfuse/node-red-dashboard node-red-contrib-postgresql node-red-node-mongodb
sudo systemctl restart nodered
```

## Importar

1. Editor → menú ☰ → **Import** → seleccionar `flows.json` → **Import**.
2. En **Global Configuration Nodes**, escribir las credenciales de:
   - `Mosquitto local` (pestaña Security, usuario `nodered`)
   - `PostgreSQL domotica_db` (usuario `domotica`)
   - `MongoDB domotica_db` (usuario `domotica`)
3. **Deploy**. El dashboard queda en `http://dashboard.domotica.lan/dashboard/inicio`.

## Pestañas del flujo

| Pestaña | Qué hace |
|---|---|
| 1 · Sensores y PostgreSQL | `casa/sensores/+` → validación de rango en el servidor → indicadores y gráficas → `INSERT` parametrizado en `lecturas`. Vigilancia: avisa si pasan 20 s sin datos. Las lecturas inválidas van a `eventos`. |
| 2 · Actuadores y control | `casa/estado/actuadores` (retenido) → interruptores del dashboard, que muestran el estado **real** del ESP32 (modo *decouple*). Los controles se bloquean en modo auto. Cada orden se publica en `casa/actuadores/...` (QoS 1, sin retener) y se audita en `eventos`. `estado_actuadores` se actualiza solo cuando algo cambia. |
| 3 · Eventos y estado del ESP32 | Alertas y errores del ESP32 → `eventos` + aviso emergente + tabla. LWT `casa/estado/esp32` → "En línea" / "Desconectado". Al arrancar, la tabla se carga con los últimos 15 eventos de MongoDB. |

Cada pestaña tiene un nodo `catch` que muestra en el panel de depuración los errores de PostgreSQL, MongoDB o de las funciones.

## Páginas del dashboard

- `/dashboard/inicio`: estado del sistema, 4 indicadores, controles de los 4 actuadores + modo + seguridad, alertas recientes.
- `/dashboard/historial`: gráficas de la última hora y registro de eventos.

> El nodo de MongoDB escribe su cadena de conexión (con la clave) en el journal. No mostrar `journalctl -u nodered` en capturas.
