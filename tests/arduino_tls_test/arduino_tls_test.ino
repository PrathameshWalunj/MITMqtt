/*
 * MITMqtt TLS Test Sketch for Wemos D1 Mini / ESP8266
 *
 * This sketch connects to the MITMqtt proxy using TLS/SSL (MQTTS)
 * on port 8883 to test encrypted MQTT interception.
 *
 * Required Libraries:
 * - PubSubClient 
 *
 * Configuration:
 * 1. Update your own WiFi credentials below
 * 2. Update MQTT_SERVER to your PC's IP address
 * 3. Upload to Wemos D1 Mini
 * 4. In MITMqtt: Enable TLS checkbox, click "Start Intercepting"
 * 5. Open Serial Monitor at 115200 baud
 */

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>


// CONFIGURATION
const char *WIFI_SSID = "your cool ssid";         
const char *WIFI_PASSWORD = "not so easy password"; 
const char *MQTT_SERVER = "IPPP";           
const int MQTT_PORT = 8883;                      


// Test configuration
const char *CLIENT_ID = "mitmqtt_tls_test";
const char *TEST_TOPIC_PUB = "mitmqtt/tls/test";
const char *TEST_TOPIC_SUB = "mitmqtt/tls/response";

// WiFi and MQTT clients
WiFiClientSecure espClient;
PubSubClient mqtt(espClient);

// Test state
int testNumber = 0;
unsigned long lastTestTime = 0;
const unsigned long TEST_INTERVAL = 3000;

// Callback for received messages
void mqttCallback(char *topic, byte *payload, unsigned int length) {
  Serial.print("[RX] Topic: ");
  Serial.print(topic);
  Serial.print(" | Payload: ");
  for (unsigned int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

void setupWiFi() {
  Serial.print("Connecting to WiFi: ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.print("Connected! IP: ");
  Serial.println(WiFi.localIP());
}

void connectMQTT() {
  while (!mqtt.connected()) {
    Serial.print("Connecting to MQTTS broker...");

    if (mqtt.connect(CLIENT_ID)) {
      Serial.println(" Connected!");

      // Subscribe to response topic
      mqtt.subscribe(TEST_TOPIC_SUB);
      Serial.print("Subscribed to: ");
      Serial.println(TEST_TOPIC_SUB);

      // Send initial connection message
      mqtt.publish(
          TEST_TOPIC_PUB,
          "{\"status\":\"TLS_CONNECTED\",\"client\":\"wemos_d1_mini\"}");
      Serial.println("Sent TLS connection confirmation");
    } else {
      Serial.print(" Failed, rc=");
      Serial.print(mqtt.state());
      Serial.println(" Retrying in 5s...");
      delay(5000);
    }
  }
}

void runTest() {
  testNumber++;

  // Create test payload
  String payload = "{\"test\":";
  payload += testNumber;
  payload += ",\"type\":\"TLS_TEST\",\"timestamp\":";
  payload += millis();
  payload += ",\"data\":\"";

  // Add varying content based on test number
  switch (testNumber % 5) {
  case 1:
    payload += "Hello from encrypted MQTT!";
    break;
  case 2:
    payload += "This message is encrypted with TLS";
    break;
  case 3:
    payload += "MITMqtt is decrypting this in real-time";
    break;
  case 4:
    payload += "Testing special chars: @#$%^&*()";
    break;
  case 0:
    payload += "Unicode test: 你好 🔐 مرحبا";
    break;
  }

  payload += "\"}";

  Serial.print("[TX] Test #");
  Serial.print(testNumber);
  Serial.print(" -> ");
  Serial.println(payload);

  mqtt.publish(TEST_TOPIC_PUB, payload.c_str());
}

void setup() {
  Serial.begin(115200);
  delay(100);

  Serial.println();
  Serial.println("========================================");
  Serial.println("   MITMqtt TLS Test - Wemos D1 Mini");
  Serial.println("========================================");
  Serial.println();

  // Setup WiFi
  setupWiFi();

  // Configure TLS - INSECURE MODE for testing
  // This accepts any certificate (including our self-signed CA)
  espClient.setInsecure();

  // If you want to verify the CA certificate, use:
  // espClient.setCACert(caCertificate); 

  // Configure MQTT
  mqtt.setServer(MQTT_SERVER, MQTT_PORT);
  mqtt.setCallback(mqttCallback);
  mqtt.setBufferSize(512);

  Serial.println();
  Serial.println("TLS Configuration:");
  Serial.print("  Server: ");
  Serial.print(MQTT_SERVER);
  Serial.print(":");
  Serial.println(MQTT_PORT);
  Serial.println("  Mode: Insecure (accept any cert)");
  Serial.println();

  // Connect to MQTT
  connectMQTT();

  Serial.println();
  Serial.println("Starting TLS test sequence...");
  Serial.println("Watch MITMqtt for [TLS] prefixed packets!");
  Serial.println();
}

void loop() {
 
  if (!mqtt.connected()) {
    connectMQTT();
  }
  mqtt.loop();

  
  unsigned long now = millis();
  if (now - lastTestTime >= TEST_INTERVAL) {
    lastTestTime = now;
    runTest();
  }
}
