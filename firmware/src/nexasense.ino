#include <WiFi.h>
#include <PubSubClient.h>
#include "DHTesp.h"

// ===== CONFIGURACAO (INSEGURA DE PROPOSITO - valores ficticios) =====
#define FW_VERSION "0.9.3-beta"
const char* WIFI_SSID = "Wokwi-GUEST";
const char* WIFI_PASS = "";
const char* MQTT_HOST = "test.mosquitto.org";
const int   MQTT_PORT = 1883;                       // sem TLS
const char* API_TOKEN = "nxh_live_9f8e7d6c5b4a3210"; // token fixo no codigo
const char* OTA_URL   = "http://update.nexahome.local/fw/nexasense.bin"; // HTTP, sem assinatura
const char* DEVICE_ID = "NXS-000123";
const char* TOPIC_TEL = "nexahome-lab/NXS-000123/telemetry";
const char* TOPIC_CMD = "nexahome-lab/NXS-000123/cmd";

WiFiClient net;
PubSubClient mqtt(net);
DHTesp dht;
unsigned long lastSend = 0;

void printConfig() {
  Serial.println("[DEBUG] ===== CONFIG =====");
  Serial.printf("[DEBUG] FW: %s | ID: %s\n", FW_VERSION, DEVICE_ID);
  Serial.printf("[DEBUG] WiFi: %s / senha: '%s'\n", WIFI_SSID, WIFI_PASS);
  Serial.printf("[DEBUG] MQTT: %s:%d\n", MQTT_HOST, MQTT_PORT);
  Serial.printf("[DEBUG] API token: %s\n", API_TOKEN);
  Serial.printf("[DEBUG] OTA: %s\n", OTA_URL);
}

void runCommand(String cmd) {
  cmd.trim();
  if (cmd == "help")   Serial.println("[DEBUG] comandos: help, config, reboot");
  if (cmd == "config") printConfig();
  if (cmd == "reboot") ESP.restart();
}

void onMessage(char* topic, byte* payload, unsigned int len) {
  String msg;
  for (unsigned int i = 0; i < len; i++) msg += (char)payload[i];
  Serial.printf("[MQTT] comando recebido: %s\n", msg.c_str());
  runCommand(msg); // aceita comando remoto sem autenticacao
}

void connectMqtt() {
  while (!mqtt.connected()) {
    Serial.printf("[DEBUG] Conectando MQTT %s:%d ...\n", MQTT_HOST, MQTT_PORT);
    if (mqtt.connect(DEVICE_ID)) {
      mqtt.subscribe(TOPIC_CMD);
      Serial.println("[DEBUG] MQTT conectado");
    } else {
      delay(2000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("NexaSense iniciando...");
  printConfig(); // vaza configuracao no boot

  dht.setup(15, DHTesp::DHT22);

  WiFi.begin(WIFI_SSID, WIFI_PASS, 6);
  while (WiFi.status() != WL_CONNECTED) { delay(250); Serial.print("."); }
  Serial.printf("\n[DEBUG] WiFi OK - IP: %s\n", WiFi.localIP().toString().c_str());

  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(onMessage);
}

void loop() {
  if (Serial.available()) runCommand(Serial.readStringUntil('\n'));
  if (!mqtt.connected()) connectMqtt();
  mqtt.loop();

  if (millis() - lastSend > 5000) {
    lastSend = millis();
    TempAndHumidity d = dht.getTempAndHumidity();
    char json[160];
    snprintf(json, sizeof(json),
      "{\"id\":\"%s\",\"token\":\"%s\",\"temp\":%.1f,\"hum\":%.1f}",
      DEVICE_ID, API_TOKEN, d.temperature, d.humidity);
    mqtt.publish(TOPIC_TEL, json);
    Serial.printf("[MQTT] enviado: %s\n", json);
  }
}