/*
 * Sistema domótico IoT — ACO941 (UDB) · Fase 4
 * Nodo ESP32 simulado en Wokwi (VS Code + Private IoT Gateway)
 *
 * Sensores (4):  DHT (temperatura y humedad), MQ-2 (gas), KS0048 (agua),
 *                KS0038 reed switch (puerta).
 * Actuadores (4): relé (foco 110 V), buzzer activo, servo SG90 (válvula),
 *                 ventilador DC (LED azul en la simulación).
 * Extras:        botón local de modo seguridad, LED de estado de conexión,
 *                alarmas críticas que actúan aunque no haya red.
 *
 * Tópicos MQTT
 *   Publica (QoS 0):           casa/sensores/{temperatura,humedad,gas,agua,puerta}
 *   Publica (retenido):        casa/estado/esp32       online | offline (LWT)
 *                              casa/estado/actuadores  JSON con el estado actual
 *   Publica:                   casa/estado/eventos     JSON de alertas (para MongoDB)
 *                              casa/estado/errores     lecturas inválidas
 *   Se suscribe (QoS 1):       casa/actuadores/#
 *       rele | buzzer | ventilador   on/off (también 1/0, true/false)
 *       servo                        0..180 (grados)
 *       modo                         auto | manual
 *       seguridad                    on | off
 *
 * Librerías (Library Manager de wokwi.com): PubSubClient,
 * DHT sensor library for ESPx, ESP32Servo.
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <DHTesp.h>
#include <ESP32Servo.h>
#include "secrets.h"   // MQTT_USER y MQTT_PASS (no se sube al repositorio)

// ---------------- Red ----------------
const char*    WIFI_SSID    = "Wokwi-GUEST";   // AP virtual de Wokwi, sin clave
const char*    WIFI_PASS    = "";
const int      WIFI_CHANNEL = 6;               // acelera la conexión en Wokwi
const char*    MQTT_HOST    = "192.168.1.233"; // Mosquitto en la VM srv-domotica
const uint16_t MQTT_PORT    = 1883;
const char*    MQTT_CLIENT  = "esp32-domotica";

// ---------------- Tópicos ----------------
#define T_TEMP      "casa/sensores/temperatura"
#define T_HUM       "casa/sensores/humedad"
#define T_GAS       "casa/sensores/gas"
#define T_AGUA      "casa/sensores/agua"
#define T_PUERTA    "casa/sensores/puerta"
#define T_STATUS    "casa/estado/esp32"
#define T_ESTADO    "casa/estado/actuadores"
#define T_EVENTOS   "casa/estado/eventos"
#define T_ERRORES   "casa/estado/errores"
#define T_CMD_ALL   "casa/actuadores/#"
#define T_CMD_BASE  "casa/actuadores/"

// ---------------- Pines ----------------
const int PIN_DHT      = 15;
const int PIN_GAS      = 34;  // ADC1 (ADC2 no funciona con WiFi activo)
const int PIN_AGUA     = 35;  // ADC1
const int PIN_PUERTA   = 27;  // HIGH = abierta
const int PIN_BTN_SEG  = 14;  // botón a GND, INPUT_PULLUP
const int PIN_RELE     = 26;
const int PIN_BUZZER   = 25;
const int PIN_SERVO    = 18;
const int PIN_VENT     = 19;
const int PIN_LED_NET  = 2;   // encendido = WiFi y MQTT conectados

// ---------------- Reglas (Fase 2) ----------------
const float TEMP_ON        = 30.0;  // °C, enciende el ventilador
const float TEMP_OFF       = 29.0;  // histéresis para que no oscile
const int   GAS_MARGEN     = 60;    // alarma si el nivel sube 60 puntos sobre el aire limpio
const int   GAS_MIN_UMBRAL = 150;   // umbral mínimo por si la calibración sale muy baja
const int   AGUA_UMBRAL    = 30;    // % 0-100
const int   SERVO_ABIERTA  = 0;     // válvula abierta
const int   SERVO_CERRADA  = 90;    // válvula cerrada

// Rangos válidos (hoja de datos del DHT11)
const float TEMP_MIN = 0.0,  TEMP_MAX = 50.0;
const float HUM_MIN  = 20.0, HUM_MAX  = 90.0;

// ---------------- Tiempos ----------------
const unsigned long INTERVALO_LECTURA = 5000;
const unsigned long REINTENTO_WIFI    = 10000;
const unsigned long REINTENTO_MQTT    = 5000;

// ---------------- Objetos ----------------
WiFiClient   wifiClient;
PubSubClient mqtt(wifiClient);
DHTesp       dht;
Servo        valvula;

// ---------------- Estado ----------------
struct Actuadores {
  bool rele = false;
  bool buzzer = false;
  bool ventilador = false;
  int  servo = SERVO_ABIERTA;
};
Actuadores act;              // estado aplicado
Actuadores manual;           // órdenes recibidas en modo manual
bool modoAuto      = true;
bool modoSeguridad = false;

float tempC = NAN, humedad = NAN;
int   gas = 0, agua = 0;
bool  puertaAbierta = false;
bool  datosValidos = false;
int   gasBase = 0, gasUmbral = 1000;   // calibración del MQ-2 en aire limpio

bool alarmaGas = false, alarmaAgua = false, alarmaIntruso = false;

unsigned long tLectura = 0, tWifi = 0, tMqtt = 0, tBuzzer = 0;
bool estadoPublicado = false;
bool buzzerSonando = false;

// =====================================================================
// Utilidades
// =====================================================================
bool esOn(const String& s) {
  return s == "on" || s == "1" || s == "true" || s == "encendido";
}

void publicar(const char* topico, const String& valor, bool retenido = false) {
  if (mqtt.connected()) {
    mqtt.publish(topico, valor.c_str(), retenido);
  }
}

void publicarEvento(const char* tipo, const char* sensor, float valor,
                    float umbral, const char* actuadores) {
  String j = String("{\"tipo\":\"") + tipo + "\",\"sensor\":\"" + sensor +
             "\",\"valor\":" + String(valor, 1) + ",\"umbral\":" +
             String(umbral, 1) + ",\"actuadores\":[" + actuadores +
             "],\"modo\":\"" + (modoAuto ? "auto" : "manual") + "\"}";
  Serial.println("[EVENTO] " + j);
  publicar(T_EVENTOS, j);
}

void publicarError(const char* sensor, const String& detalle) {
  String j = String("{\"sensor\":\"") + sensor + "\",\"error\":\"" + detalle + "\"}";
  Serial.println("[ERROR] " + j);
  publicar(T_ERRORES, j);
}

void publicarEstado() {
  String j = String("{\"rele\":\"") + (act.rele ? "on" : "off") +
             "\",\"buzzer\":\"" + (act.buzzer ? "on" : "off") +
             "\",\"ventilador\":\"" + (act.ventilador ? "on" : "off") +
             "\",\"servo\":" + act.servo +
             ",\"modo\":\"" + (modoAuto ? "auto" : "manual") +
             "\",\"seguridad\":\"" + (modoSeguridad ? "on" : "off") + "\"}";
  Serial.println("[ESTADO] " + j);
  publicar(T_ESTADO, j, true);
  estadoPublicado = true;
}

// =====================================================================
// Conectividad (no bloqueante: las reglas siguen funcionando sin red)
// =====================================================================
void gestionarWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  unsigned long ahora = millis();
  if (tWifi != 0 && ahora - tWifi < REINTENTO_WIFI) return;
  tWifi = ahora;
  Serial.println("[WIFI] Conectando a " + String(WIFI_SSID) + "...");
  WiFi.disconnect();
  WiFi.begin(WIFI_SSID, WIFI_PASS, WIFI_CHANNEL);
}

void alRecibir(char* topic, byte* payload, unsigned int len);

void gestionarMQTT() {
  if (WiFi.status() != WL_CONNECTED) return;
  if (mqtt.connected()) return;
  unsigned long ahora = millis();
  if (tMqtt != 0 && ahora - tMqtt < REINTENTO_MQTT) return;
  tMqtt = ahora;

  Serial.print("[MQTT] Conectando a " + String(MQTT_HOST) + ":" + MQTT_PORT + "... ");
  // LWT: si el ESP32 se cae, el broker publica "offline" (retenido, QoS 1)
  bool ok = mqtt.connect(MQTT_CLIENT, MQTT_USER, MQTT_PASS,
                         T_STATUS, 1, true, "offline");
  if (ok) {
    Serial.println("conectado");
    mqtt.publish(T_STATUS, "online", true);
    mqtt.subscribe(T_CMD_ALL, 1);   // órdenes con QoS 1: no se deben perder
    publicarEstado();
  } else {
    // -4 timeout, -2 sin conexión TCP, 4 usuario/clave, 5 no autorizado (ACL)
    Serial.println("fallo, rc=" + String(mqtt.state()) +
                   ", reintento en " + String(REINTENTO_MQTT / 1000) + " s");
  }
}

// =====================================================================
// Órdenes manuales desde el dashboard
// =====================================================================
void alRecibir(char* topic, byte* payload, unsigned int len) {
  String t = String(topic);
  String v;
  for (unsigned int i = 0; i < len; i++) v += (char)payload[i];
  v.trim();
  v.toLowerCase();
  Serial.println("[CMD] " + t + " = " + v);

  if (!t.startsWith(T_CMD_BASE)) return;
  String disp = t.substring(strlen(T_CMD_BASE));

  if (disp == "modo") {
    if (v == "auto" || v == "manual") {
      modoAuto = (v == "auto");
      if (!modoAuto) manual = act;   // el modo manual parte del estado actual
    } else {
      publicarError("cmd", "modo invalido: " + v);
    }
  } else if (disp == "seguridad") {
    modoSeguridad = esOn(v);
  } else if (disp == "rele" || disp == "buzzer" || disp == "ventilador" || disp == "servo") {
    if (modoAuto) {
      publicarError("cmd", disp + " ignorado: el sistema esta en modo auto");
      return;
    }
    if (disp == "rele")       manual.rele = esOn(v);
    else if (disp == "buzzer")     manual.buzzer = esOn(v);
    else if (disp == "ventilador") manual.ventilador = esOn(v);
    else {
      int ang = v.toInt();
      if (ang < 0 || ang > 180 || (ang == 0 && v != "0")) {
        publicarError("cmd", "angulo de servo invalido: " + v);
        return;
      }
      manual.servo = ang;
    }
  } else {
    publicarError("cmd", "dispositivo desconocido: " + disp);
    return;
  }
  estadoPublicado = false;   // forzar publicación del nuevo estado
}

// =====================================================================
// Lectura y validación de sensores
// =====================================================================
void leerSensores() {
  datosValidos = true;

  TempAndHumidity d = dht.getTempAndHumidity();
  if (dht.getStatus() != DHTesp::ERROR_NONE || isnan(d.temperature) || isnan(d.humidity)) {
    publicarError("DHT11", String("sin lectura: ") + dht.getStatusString());
    datosValidos = false;
  } else {
    if (d.temperature < TEMP_MIN || d.temperature > TEMP_MAX) {
      publicarError("DHT11", "temperatura fuera de rango: " + String(d.temperature, 1));
      datosValidos = false;
    } else {
      tempC = d.temperature;
      publicar(T_TEMP, String(tempC, 1));
    }
    if (d.humidity < HUM_MIN || d.humidity > HUM_MAX) {
      publicarError("DHT11", "humedad fuera de rango: " + String(d.humidity, 1));
    } else {
      humedad = d.humidity;
      publicar(T_HUM, String(humedad, 1));
    }
  }

  int crudoGas = analogRead(PIN_GAS);
  int crudoAgua = analogRead(PIN_AGUA);
  if (crudoGas < 0 || crudoGas > 4095) {
    publicarError("MQ2", "ADC fuera de rango: " + String(crudoGas));
  } else {
    gas = map(crudoGas, 0, 4095, 0, 1000);
    publicar(T_GAS, String(gas));
  }
  if (crudoAgua < 0 || crudoAgua > 4095) {
    publicarError("KS0048", "ADC fuera de rango: " + String(crudoAgua));
  } else {
    agua = map(crudoAgua, 0, 4095, 0, 100);
    publicar(T_AGUA, String(agua));
  }

  publicar(T_PUERTA, puertaAbierta ? "1" : "0");

  Serial.printf("[LECTURA] T=%.1f C  H=%.1f %%  gas=%d/%d  agua=%d %%  puerta=%s  modo=%s  seg=%s\n",
                tempC, humedad, gas, gasUmbral, agua, puertaAbierta ? "abierta" : "cerrada",
                modoAuto ? "auto" : "manual", modoSeguridad ? "on" : "off");
}

// La puerta y el botón se revisan en cada vuelta para reaccionar al instante
void revisarEntradasRapidas() {
  bool p = digitalRead(PIN_PUERTA) == HIGH;
  if (p != puertaAbierta) {
    puertaAbierta = p;
    Serial.println(String("[PUERTA] ") + (p ? "abierta" : "cerrada"));
    publicar(T_PUERTA, p ? "1" : "0");
  }

  static bool btnPrevio = HIGH;
  static unsigned long tBtn = 0;
  bool b = digitalRead(PIN_BTN_SEG);
  if (b != btnPrevio && millis() - tBtn > 50) {
    tBtn = millis();
    btnPrevio = b;
    if (b == LOW) {
      modoSeguridad = !modoSeguridad;
      Serial.println(String("[BOTON] Modo seguridad ") + (modoSeguridad ? "ON" : "OFF"));
      estadoPublicado = false;
    }
  }
}

// =====================================================================
// Reglas de control
// =====================================================================
void evaluarReglas() {
  Actuadores nuevo = modoAuto ? Actuadores() : manual;

  // Regla 1: temperatura > 30 °C -> ventilador (solo en auto, con histéresis)
  if (modoAuto && !isnan(tempC)) {
    static bool calor = false;
    if (tempC > TEMP_ON) calor = true;
    else if (tempC < TEMP_OFF) calor = false;
    if (calor) nuevo.ventilador = true;
  }

  // Reglas críticas: actúan también en modo manual (seguridad de las personas)
  // Regla 2: gas -> buzzer + ventilador
  bool g = gas > gasUmbral;
  if (g) { nuevo.buzzer = true; nuevo.ventilador = true; }
  if (g && !alarmaGas) publicarEvento("alerta_gas", "MQ2", gas, gasUmbral, "\"buzzer\",\"ventilador\"");
  alarmaGas = g;

  // Regla 3: agua -> buzzer + servo a 90° (cierra la válvula)
  bool w = agua > AGUA_UMBRAL;
  if (w) { nuevo.buzzer = true; nuevo.servo = SERVO_CERRADA; }
  if (w && !alarmaAgua) publicarEvento("fuga_agua", "KS0048", agua, AGUA_UMBRAL, "\"buzzer\",\"servo\"");
  alarmaAgua = w;

  // Regla 4: puerta abierta y modo seguridad -> buzzer + relé (foco)
  bool i = puertaAbierta && modoSeguridad;
  if (i) { nuevo.buzzer = true; nuevo.rele = true; }
  if (i && !alarmaIntruso) publicarEvento("puerta_abierta", "KS0038", 1, 1, "\"buzzer\",\"rele\"");
  alarmaIntruso = i;

  if (nuevo.rele != act.rele || nuevo.buzzer != act.buzzer ||
      nuevo.ventilador != act.ventilador || nuevo.servo != act.servo) {
    act = nuevo;
    estadoPublicado = false;
  }
}

void aplicarActuadores() {
  digitalWrite(PIN_RELE, act.rele ? HIGH : LOW);
  digitalWrite(PIN_VENT, act.ventilador ? HIGH : LOW);

  static int servoPrevio = -1;
  if (act.servo != servoPrevio) {
    valvula.write(act.servo);
    servoPrevio = act.servo;
  }

  // Buzzer intermitente (250 ms) para que suene como alarma
  if (act.buzzer) {
    if (millis() - tBuzzer >= 250) {
      tBuzzer = millis();
      buzzerSonando = !buzzerSonando;
      if (buzzerSonando) tone(PIN_BUZZER, 2000);
      else noTone(PIN_BUZZER);
    }
  } else if (buzzerSonando) {
    noTone(PIN_BUZZER);
    buzzerSonando = false;
  }

  digitalWrite(PIN_LED_NET, mqtt.connected() ? HIGH : LOW);
}

// =====================================================================
void setup() {
  Serial.begin(115200);
  Serial.println("\n=== Nodo ESP32 domotica ACO941 ===");

  pinMode(PIN_PUERTA, INPUT);
  pinMode(PIN_BTN_SEG, INPUT_PULLUP);
  pinMode(PIN_RELE, OUTPUT);
  pinMode(PIN_VENT, OUTPUT);
  pinMode(PIN_LED_NET, OUTPUT);
  analogReadResolution(12);

  // El servo se asigna antes que el buzzer para que no compartan canal PWM
  valvula.setPeriodHertz(50);
  valvula.attach(PIN_SERVO, 500, 2400);
  valvula.write(SERVO_ABIERTA);

  dht.setup(PIN_DHT, DHTesp::DHT22);  // Wokwi solo trae DHT22; el código valida con rangos del DHT11

  WiFi.mode(WIFI_STA);
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(alRecibir);
  mqtt.setBufferSize(512);
  mqtt.setKeepAlive(15);              // el broker detecta la caída en ~22 s y envía el LWT

  puertaAbierta = digitalRead(PIN_PUERTA) == HIGH;

  // Calibración del MQ-2: promedio de 20 lecturas en aire limpio (como el R0 del sensor real)
  long suma = 0;
  for (int k = 0; k < 20; k++) { suma += analogRead(PIN_GAS); delay(20); }
  gasBase = map(suma / 20, 0, 4095, 0, 1000);
  gasUmbral = max(gasBase + GAS_MARGEN, GAS_MIN_UMBRAL);
  if (gasUmbral > 990) gasUmbral = 990;
  Serial.printf("[MQ2] Calibrado: base=%d  umbral de alarma=%d\n", gasBase, gasUmbral);
}

void loop() {
  static wl_status_t wifiPrevio = WL_IDLE_STATUS;
  wl_status_t ws = WiFi.status();
  if (ws != wifiPrevio) {
    if (ws == WL_CONNECTED)
      Serial.println("[WIFI] Conectado, IP " + WiFi.localIP().toString());
    else if (wifiPrevio == WL_CONNECTED)
      Serial.println("[WIFI] Conexion perdida");
    wifiPrevio = ws;
  }

  gestionarWiFi();
  gestionarMQTT();
  mqtt.loop();

  revisarEntradasRapidas();

  if (millis() - tLectura >= INTERVALO_LECTURA || tLectura == 0) {
    tLectura = millis();
    leerSensores();
  }

  evaluarReglas();
  aplicarActuadores();

  if (!estadoPublicado && mqtt.connected()) publicarEstado();

  delay(10);
}
