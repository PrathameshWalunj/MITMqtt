/*
 * MITMqtt TLS Test Sketch for Wemos D1 Mini / ESP8266
 *
 *  Set Serial Monitor to 115200 baud
 */

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>


// ========== UPDATE THESE ==========
const char *WIFI_SSID = "your cool ssid";         
const char *WIFI_PASSWORD = "not so easy password"; 
const char *MQTT_SERVER = "IPPP";          
// ==================================

const int MQTT_PORT = 8883; // MQTTS/TLS port
const char *CLIENT_ID = "wemos_tls_test";
const char *TEST_TOPIC = "mitmqtt/tls/test";

WiFiClientSecure espClient;
PubSubClient mqtt(espClient);

int testNum = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("=== MITMqtt TLS Test ===");
  Serial.println();

  // Connect WiFi
  Serial.print("WiFi: ");
  Serial.println(WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nWiFi FAILED! Check credentials.");
    while (1)
      delay(1000);
  }

  Serial.println();
  Serial.print("Connected! IP: ");
  Serial.println(WiFi.localIP());

  // TLS setup - accept any certificate
  espClient.setInsecure();

  // MQTT setup
  mqtt.setServer(MQTT_SERVER, MQTT_PORT);
  mqtt.setBufferSize(512);

  Serial.print("MQTT: ");
  Serial.print(MQTT_SERVER);
  Serial.print(":");
  Serial.println(MQTT_PORT);
}

void connectMQTT() {
  Serial.print("Connecting MQTTS...");

  if (mqtt.connect(CLIENT_ID)) {
    Serial.println(" OK!");
    mqtt.publish(TEST_TOPIC, "{\"msg\":\"TLS Connected!\"}");
  } else {
    Serial.print(" FAIL rc=");
    Serial.println(mqtt.state());
  }
}

void loop() {
  if (!mqtt.connected()) {
    connectMQTT();
    delay(5000);
    return;
  }

  mqtt.loop();

  
  static unsigned long lastMsg = 0;
  if (millis() - lastMsg > 3000) {
    lastMsg = millis();
    testNum++;

    String msg = "{\"test\":";
    msg += testNum;
    msg += ",\"type\":\"TLS\"}";

    Serial.print("TX: ");
    Serial.println(msg);
    mqtt.publish(TEST_TOPIC, msg.c_str());
  }
}
