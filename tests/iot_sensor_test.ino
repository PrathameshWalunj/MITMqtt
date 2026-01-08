/*
 * MITMqtt Real-World TLS Test - IoT Sensor Simulation
 *
 * Test Plan:
 * 1. Wemos publishes sensor data -> MITMqtt intercepts
 * 2. From MITMqtt, inject a command -> Wemos receives and responds
 */

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>


// ========== UPDATE THESE ==========
const char *WIFI_SSID = "your cool ssid";
const char *WIFI_PASSWORD = "not so easy password";
const char *MQTT_SERVER = "IPPP";
// ==================================

const int MQTT_PORT = 8883;
const char *CLIENT_ID = "iot_sensor_01";
const char *TOPIC_SENSOR = "home/sensor/living_room";
const char *TOPIC_COMMAND = "home/sensor/living_room/cmd";
const char *TOPIC_STATUS = "home/sensor/living_room/status";

WiFiClientSecure espClient;
PubSubClient mqtt(espClient);

float temperature = 22.5;
float humidity = 45.0;
bool ledState = false;
int commandsReceived = 0;

void callback(char *topic, byte *payload, unsigned int length) {
  char msg[256];
  for (unsigned int i = 0; i < length && i < 255; i++) {
    msg[i] = (char)payload[i];
  }
  msg[length < 255 ? length : 255] = '\0';

  Serial.println();
  Serial.println("========== COMMAND RECEIVED ==========");
  Serial.print("Topic: ");
  Serial.println(topic);
  Serial.print("Payload: ");
  Serial.println(msg);
  Serial.println("=======================================");

  commandsReceived++;

  if (strstr(msg, "led_on")) {
    ledState = true;
    mqtt.publish(TOPIC_STATUS, "{\"response\":\"LED turned ON\",\"led\":true}");
    Serial.println("-> LED ON!");
  } else if (strstr(msg, "led_off")) {
    ledState = false;
    mqtt.publish(TOPIC_STATUS,
                 "{\"response\":\"LED turned OFF\",\"led\":false}");
    Serial.println("-> LED OFF!");
  } else if (strstr(msg, "status")) {
    char buf[200];
    snprintf(buf, sizeof(buf),
             "{\"temp\":%.1f,\"humidity\":%.1f,\"led\":%s,\"uptime\":%lu}",
             temperature, humidity, ledState ? "true" : "false",
             millis() / 1000);
    mqtt.publish(TOPIC_STATUS, buf);
    Serial.println("-> Status sent!");
  } else {
    mqtt.publish(TOPIC_STATUS, "{\"error\":\"Unknown command\"}");
    Serial.println("-> Unknown command!");
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("=== MITMqtt IoT Sensor Test ===");
  Serial.println("Commands: led_on, led_off, status");
  Serial.println();

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.print(" IP: ");
  Serial.println(WiFi.localIP());

  espClient.setInsecure();
  mqtt.setServer(MQTT_SERVER, MQTT_PORT);
  mqtt.setCallback(callback);
  mqtt.setBufferSize(512);

  Serial.print("Publish: ");
  Serial.println(TOPIC_SENSOR);
  Serial.print("Subscribe: ");
  Serial.println(TOPIC_COMMAND);
}

void connectMQTT() {
  while (!mqtt.connected()) {
    Serial.print("MQTTS...");
    if (mqtt.connect(CLIENT_ID)) {
      Serial.println(" OK!");
      mqtt.subscribe(TOPIC_COMMAND);
      mqtt.publish(TOPIC_STATUS, "{\"event\":\"connected\"}");
    } else {
      Serial.print(" FAIL rc=");
      Serial.println(mqtt.state());
      delay(5000);
    }
  }
}

void publishSensorData() {
  temperature += random(-10, 11) / 10.0;
  humidity += random(-20, 21) / 10.0;
  temperature = constrain(temperature, 18.0, 28.0);
  humidity = constrain(humidity, 30.0, 70.0);

  char payload[150];
  snprintf(payload, sizeof(payload),
           "{\"device\":\"sensor_01\",\"temp\":%.1f,\"humidity\":%.1f,\"led\":%"
           "s,\"uptime\":%lu}",
           temperature, humidity, ledState ? "true" : "false", millis() / 1000);

  Serial.print("[TX] ");
  Serial.println(payload);
  mqtt.publish(TOPIC_SENSOR, payload);
}

void loop() {
  if (!mqtt.connected())
    connectMQTT();
  mqtt.loop();

  static unsigned long lastPub = 0;
  if (millis() - lastPub >= 5000) {
    lastPub = millis();
    publishSensorData();
  }
}
