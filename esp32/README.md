# Nodo ESP32 (simulado en Wokwi) · Fase 4 ACO941

ESP32 con 4 sensores y 4 actuadores que publica en el Mosquitto de `srv-domotica` (192.168.1.233:1883) y recibe órdenes del dashboard por MQTT. Se simula con **Wokwi para VS Code**, cuyo Private IoT Gateway integrado saca el tráfico del ESP32 virtual por la red de la laptop, así llega a la IP privada de la VM.

## Archivos

| Archivo | Para qué sirve |
|---|---|
| `sketch.ino` | Programa del ESP32 (Arduino) |
| `diagram.json` | Circuito de Wokwi |
| `wokwi.toml` | Le dice a Wokwi para VS Code qué firmware ejecutar |
| `libraries.txt` | Librerías que instala wokwi.com al compilar |
| `secrets.example.h` | Plantilla de credenciales; la copia real `secrets.h` no se sube |
| `.gitignore` | Excluye `secrets.h` y el firmware `.bin` (lleva la clave dentro) |

## Hardware simulado

| Elemento real | En Wokwi | GPIO |
|---|---|---|
| DHT11 (temp./humedad) | DHT22 (Wokwi no trae DHT11; el código valida con los rangos del DHT11) | 15 |
| MQ-2 (gas) | Gas sensor, salida AOUT | 34 (ADC1) |
| KS0048 (agua) | Potenciómetro deslizante (0–100 %) | 35 (ADC1) |
| KS0038 reed switch (puerta) | Interruptor deslizante (alto = abierta) | 27 |
| Relé + foco 110 V | Relay module + LED amarillo | 26 |
| Buzzer | Buzzer (suena intermitente) | 25 |
| Servo SG90 (válvula) | Servo | 18 |
| Ventilador DC | LED azul | 19 |
| Extra: botón modo seguridad | Pulsador amarillo | 14 |
| Extra: LED de conexión MQTT | LED verde | 2 |

Se usan ADC1 (GPIO 34/35) porque ADC2 no funciona con WiFi activo.

## Tópicos MQTT

| Tópico | Sentido | QoS / retención | Contenido |
|---|---|---|---|
| `casa/sensores/temperatura` · `humedad` · `gas` · `agua` · `puerta` | ESP32 → broker | QoS 0 | número (puerta: 1 abierta, 0 cerrada) |
| `casa/estado/esp32` | ESP32 → broker | QoS 1, retenido, **LWT** | `online` / `offline` |
| `casa/estado/actuadores` | ESP32 → broker | retenido | JSON con relé, buzzer, ventilador, servo, modo y seguridad |
| `casa/estado/eventos` | ESP32 → broker | QoS 0 | JSON de alerta (mismo formato que la colección `eventos` de MongoDB) |
| `casa/estado/errores` | ESP32 → broker | QoS 0 | lecturas u órdenes inválidas |
| `casa/actuadores/rele` · `buzzer` · `ventilador` | dashboard → ESP32 | suscrito QoS 1 | `on` / `off` |
| `casa/actuadores/servo` | dashboard → ESP32 | suscrito QoS 1 | 0–180 |
| `casa/actuadores/modo` | dashboard → ESP32 | suscrito QoS 1 | `auto` / `manual` |
| `casa/actuadores/seguridad` | dashboard → ESP32 | suscrito QoS 1 | `on` / `off` |

Por qué ese QoS: la telemetría se repite cada 5 s, así que perder una muestra no importa (QoS 0, menos tráfico). Las órdenes y el estado del nodo no se deben perder (suscripción QoS 1 y mensajes retenidos). PubSubClient solo publica en QoS 0; el LWT sí va en QoS 1.

## Lógica

- **Modo auto:** las 4 reglas de la Fase 2. Temperatura > 30 °C enciende el ventilador (se apaga bajo 29 °C, histéresis). Gas sobre el umbral calibrado al arrancar (nivel en aire limpio + 60) activa buzzer y ventilador. Agua > 30 % activa buzzer y cierra la válvula (servo a 90°). Puerta abierta con modo seguridad activo activa buzzer y relé.
- **Modo manual:** los actuadores obedecen al dashboard. Las alarmas de **gas y agua siguen actuando** aunque esté en manual, por seguridad.
- **Manejo de errores:** reconexión WiFi cada 10 s y MQTT cada 5 s sin bloquear el programa (las reglas siguen funcionando sin red), validación de rangos del DHT11 con aviso en `casa/estado/errores`, órdenes inválidas rechazadas, LWT `offline` si el nodo se cae.

## Cómo ejecutarlo (sin instalar PlatformIO ni Arduino CLI)

1. **Servidor:** permitir al usuario `esp32` escribir en `casa/estado/#` (ver abajo).
2. **wokwi.com:** crear un proyecto ESP32 Arduino, pegar `sketch.ino`, `diagram.json` y `libraries.txt`, añadir una pestaña `secrets.h` con la clave real y pulsar ▶ para compilar. En el editor de código, F1 → **Download compiled firmware** → guardar como `sketch.bin`.
3. **Windows:** poner `diagram.json`, `wokwi.toml` y `sketch.bin` en una carpeta, abrirla en VS Code y ejecutar F1 → **Wokwi: Start Simulator**. Dejar visible la pestaña del simulador.
4. **Verificar** en la VM con `mosquitto_sub -h localhost -u nodered -P "$MQTT_PASS" -t 'casa/#' -v`.

Si se cambia el código, repetir el paso 2 y reemplazar `sketch.bin`.

### ACL de Mosquitto (paso 1)

`/etc/mosquitto/acl` queda así:

```
user esp32
topic write casa/sensores/#
topic write casa/estado/#
topic read  casa/actuadores/#

user nodered
topic readwrite casa/#
```

Sin esa línea Mosquitto rechaza la conexión del ESP32, porque el LWT apunta a `casa/estado/esp32`.
