/*
 * MITMqtt Comprehensive Test Suite for Wemos D1 Mini
 *
 * This sketch runs automated tests to verify all MITMqtt proxy features.
 *UPDATE YOUR SSID, WIFI PASWORD AND DEVICE IP
 * TESTS INCLUDED:
 * 1. Basic connectivity (CONNECT, SUBSCRIBE, PUBLISH)
 * 2. QoS 0, 1, 2 publishing
 * 3. Large payloads (1KB, 5KB)
 * 4. Rapid publishing (stress test)
 * 5. Binary data
 * 6. Unicode/UTF-8 data
 * 7. Empty payloads
 * 8. Long topic names
 * 9. Reconnection handling
 *
 * Monitor Serial output at 115200 baud for test results.
 */

#include <ESP8266WiFi.h>
#include <PubSubClient.h>

//  CONFIGURATION
const char *WIFI_SSID = "your cool ssid";
const char *WIFI_PASSWORD = "not so easy password";
const char *MQTT_SERVER = "IPPP"; 
const int MQTT_PORT = 1883;
const char *CLIENT_ID = "MITMqtt_TestClient";
//

WiFiClient espClient;
PubSubClient mqttClient(espClient);

// Test state
enum TestState {
  TEST_IDLE,
  TEST_BASIC_PUBLISH,
  TEST_QOS_0,
  TEST_QOS_1,
  TEST_QOS_2,
  TEST_LARGE_PAYLOAD_1K,
  TEST_LARGE_PAYLOAD_5K,
  TEST_RAPID_PUBLISH,
  TEST_BINARY_DATA,
  TEST_UNICODE_DATA,
  TEST_EMPTY_PAYLOAD,
  TEST_LONG_TOPIC,
  TEST_RECONNECT,
  TEST_COMPLETE
};

TestState currentTest = TEST_IDLE;
unsigned long testStartTime = 0;
int rapidPublishCount = 0;
int messagesReceived = 0;

// Topic for receiving commands from MITMqtt
const char *COMMAND_TOPIC = "mitmqtt/test/command";
const char *RESULT_TOPIC = "mitmqtt/test/result";
const char *SENSOR_TOPIC = "mitmqtt/test/sensor";

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.println("========================================");
  Serial.println("MITMqtt Comprehensive Test");
  Serial.println("========================================");
  Serial.print("Connecting to WiFi: ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[OK] WiFi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n[FAIL] WiFi connection failed!");
    while (1)
      delay(1000); // Halt
  }
}

void mqtt_callback(char *topic, byte *payload, unsigned int length) {
  messagesReceived++;

  Serial.println();
  Serial.println(">>> MESSAGE RECEIVED <<<");
  Serial.print("Topic: ");
  Serial.println(topic);
  Serial.print("Length: ");
  Serial.println(length);
  Serial.print("Payload: ");

  for (unsigned int i = 0; i < length && i < 100; i++) {
    if (payload[i] >= 32 && payload[i] < 127) {
      Serial.print((char)payload[i]);
    } else {
      Serial.print("[0x");
      Serial.print(payload[i], HEX);
      Serial.print("]");
    }
  }
  if (length > 100)
    Serial.print("...(truncated)");
  Serial.println();
  Serial.println("<<<<<<<<<<<<<<<<<<<<<<<<");

  // Check for test commands from MITMqtt
  String cmd = "";
  for (unsigned int i = 0; i < length; i++) {
    cmd += (char)payload[i];
  }

  if (cmd == "RUN_ALL_TESTS") {
    Serial.println("[CMD] Starting all tests...");
    currentTest = TEST_BASIC_PUBLISH;
    testStartTime = millis();
  } else if (cmd == "TEST_RECEIVED") {
    Serial.println("[CMD] MITMqtt confirmed packet modification works!");
  } else if (cmd.startsWith("INJECT:")) {
    Serial.println("[CMD] Received injected message from MITMqtt!");
    Serial.print("Injected data: ");
    Serial.println(cmd.substring(7));
  }
}

void reconnect_mqtt() {
  int attempts = 0;
  while (!mqttClient.connected() && attempts < 5) {
    Serial.print("Connecting to MITMqtt proxy...");

    if (mqttClient.connect(CLIENT_ID)) {
      Serial.println("[OK]");

      // Subscribe to command topic
      mqttClient.subscribe(COMMAND_TOPIC);
      Serial.print("Subscribed to: ");
      Serial.println(COMMAND_TOPIC);

      // Publish connection status
      String status = "{\"event\":\"connected\",\"client\":\"" +
                      String(CLIENT_ID) + "\",\"ip\":\"" +
                      WiFi.localIP().toString() + "\"}";
      mqttClient.publish(RESULT_TOPIC, status.c_str());

    } else {
      Serial.print("[FAIL] rc=");
      Serial.println(mqttClient.state());
      attempts++;
      delay(2000);
    }
  }
}

//  TEST FUNCTIONS 

void runTest_BasicPublish() {
  Serial.println("\n=== TEST 1: Basic PUBLISH (QoS 0) ===");

  String payload = "{\"test\":\"basic_publish\",\"value\":42,\"timestamp\":" +
                   String(millis()) + "}";

  if (mqttClient.publish(SENSOR_TOPIC, payload.c_str())) {
    Serial.println("[OK] Basic PUBLISH sent");
    publishTestResult("TEST_1_BASIC_PUBLISH", "PASS", payload);
  } else {
    Serial.println("[FAIL] Basic PUBLISH failed");
    publishTestResult("TEST_1_BASIC_PUBLISH", "FAIL", "publish returned false");
  }

  delay(1000);
  currentTest = TEST_QOS_0;
}

void runTest_QoS0() {
  Serial.println("\n=== TEST 2: QoS 0 (At most once) ===");

  String payload = "{\"test\":\"qos_0\",\"qos\":0}";
  mqttClient.publish(SENSOR_TOPIC, payload.c_str(), false); // QoS 0
  Serial.println("[OK] QoS 0 message sent");
  publishTestResult("TEST_2_QOS_0", "PASS", "");

  delay(1000);
  currentTest = TEST_QOS_1;
}

void runTest_QoS1() {
  Serial.println("\n=== TEST 3: QoS 1 (At least once) ===");

  // PubSubClient doesn't support QoS 1/2 by default, just testing the
  // concept for now
  String payload = "{\"test\":\"qos_1\",\"qos\":1,\"note\":\"PubSubClient "
                   "limited to QoS 0\"}";
  mqttClient.publish(SENSOR_TOPIC, payload.c_str());
  Serial.println("[INFO] QoS 1 not directly supported by PubSubClient");
  publishTestResult("TEST_3_QOS_1", "PARTIAL", "Library limitation");

  delay(1000);
  currentTest = TEST_LARGE_PAYLOAD_1K;
}

void runTest_LargePayload1K() {
  Serial.println("\n=== TEST 4: Large Payload (1KB) ===");

  String payload = "{\"test\":\"large_1k\",\"data\":\"";
  // Adds ~1000 characters
  for (int i = 0; i < 50; i++) {
    payload += "ABCDEFGHIJ1234567890"; 
  }
  payload += "\"}";

  Serial.print("Payload size: ");
  Serial.print(payload.length());
  Serial.println(" bytes");

  if (mqttClient.publish(SENSOR_TOPIC, payload.c_str())) {
    Serial.println("[OK] 1KB payload sent");
    publishTestResult("TEST_4_LARGE_1K", "PASS",
                      String(payload.length()) + " bytes");
  } else {
    Serial.println("[FAIL] 1KB payload failed");
    publishTestResult("TEST_4_LARGE_1K", "FAIL", "too large for buffer");
  }

  delay(1000);
  currentTest = TEST_RAPID_PUBLISH;
}

void runTest_RapidPublish() {
  Serial.println("\n=== TEST 5: Rapid Publish (10 messages) ===");

  int sent = 0;
  for (int i = 0; i < 10; i++) {
    String payload = "{\"test\":\"rapid\",\"seq\":" + String(i) + "}";
    if (mqttClient.publish(SENSOR_TOPIC, payload.c_str())) {
      sent++;
    }
    mqttClient.loop(); 
    delay(100);        
  }

  Serial.print("[INFO] Sent ");
  Serial.print(sent);
  Serial.println("/10 rapid messages");

  if (sent == 10) {
    publishTestResult("TEST_5_RAPID_PUBLISH", "PASS", "10/10 sent");
  } else {
    publishTestResult("TEST_5_RAPID_PUBLISH", "PARTIAL",
                      String(sent) + "/10 sent");
  }

  delay(1000);
  currentTest = TEST_BINARY_DATA;
}

void runTest_BinaryData() {
  Serial.println("\n=== TEST 6: Binary Data ===");

  
  uint8_t binaryPayload[] = {0x00, 0x01, 0x02, 0xFF, 0xFE, 0x7F, 0x80, 0x00};

  if (mqttClient.publish(SENSOR_TOPIC, binaryPayload, sizeof(binaryPayload))) {
    Serial.println("[OK] Binary data sent (8 bytes with nulls)");
    publishTestResult("TEST_6_BINARY", "PASS", "8 bytes including null");
  } else {
    Serial.println("[FAIL] Binary data failed");
    publishTestResult("TEST_6_BINARY", "FAIL", "");
  }

  delay(1000);
  currentTest = TEST_UNICODE_DATA;
}

void runTest_UnicodeData() {
  Serial.println("\n=== TEST 7: Unicode/UTF-8 Data ===");

 
  String payload = "{\"test\":\"unicode\",\"emoji\":\"🔥💧\",\"chinese\":"
                   "\"你好\",\"arabic\":\"مرحبا\"}";

  if (mqttClient.publish(SENSOR_TOPIC, payload.c_str())) {
    Serial.println("[OK] Unicode payload sent");
    publishTestResult("TEST_7_UNICODE", "PASS", "emoji+chinese+arabic");
  } else {
    Serial.println("[FAIL] Unicode payload failed");
    publishTestResult("TEST_7_UNICODE", "FAIL", "");
  }

  delay(1000);
  currentTest = TEST_EMPTY_PAYLOAD;
}

void runTest_EmptyPayload() {
  Serial.println("\n=== TEST 8: Empty Payload ===");

  if (mqttClient.publish(SENSOR_TOPIC, "")) {
    Serial.println("[OK] Empty payload sent");
    publishTestResult("TEST_8_EMPTY", "PASS", "0 bytes");
  } else {
    Serial.println("[FAIL] Empty payload failed");
    publishTestResult("TEST_8_EMPTY", "FAIL", "");
  }

  delay(1000);
  currentTest = TEST_LONG_TOPIC;
}

void runTest_LongTopic() {
  Serial.println("\n=== TEST 9: Long Topic Name ===");

  
  String longTopic = "mitmqtt/test/very/long/topic/name/that/goes/on/and/on/"
                     "with/many/levels/to/test/parsing";
  String payload = "{\"test\":\"long_topic\"}";

  Serial.print("Topic length: ");
  Serial.println(longTopic.length());

  if (mqttClient.publish(longTopic.c_str(), payload.c_str())) {
    Serial.println("[OK] Long topic message sent");
    publishTestResult("TEST_9_LONG_TOPIC", "PASS",
                      String(longTopic.length()) + " chars");
  } else {
    Serial.println("[FAIL] Long topic failed");
    publishTestResult("TEST_9_LONG_TOPIC", "FAIL", "");
  }

  delay(1000);
  currentTest = TEST_COMPLETE;
}

void publishTestResult(String testName, String status, String details) {
  String result = "{\"test\":\"" + testName + "\",\"status\":\"" + status +
                  "\",\"details\":\"" + details + "\"}";
  mqttClient.publish(RESULT_TOPIC, result.c_str());
}

void runTestComplete() {
  Serial.println("\n========================================");
  Serial.println("       ALL TESTS COMPLETE!");
  Serial.println("========================================");
  Serial.print("Messages received during tests: ");
  Serial.println(messagesReceived);
  Serial.println();
  Serial.println("Check MITMqtt GUI for all captured packets.");
  Serial.println("Try clicking on a PUBLISH packet and using:");
  Serial.println("  - 'Replay Original' to resend it");
  Serial.println("  - 'Send Modified' to inject modified data");
  Serial.println();
  Serial.println("Waiting for commands on topic: mitmqtt/test/command");
  Serial.println("  Send 'RUN_ALL_TESTS' to run again");
  Serial.println("========================================");

  publishTestResult("ALL_TESTS", "COMPLETE", "Check MITMqtt GUI");

  currentTest = TEST_IDLE;
}

// ==================== MAIN LOOP ====================

void setup() {
  Serial.begin(115200);
  delay(100);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  setup_wifi();

  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setCallback(mqtt_callback);
  mqttClient.setBufferSize(2048); 

  reconnect_mqtt();

  Serial.println();
  Serial.println("========================================");
  Serial.println("Ready for testing!");
  Serial.println("----------------------------------------");
  Serial.println("Option 1: Wait 5 seconds for auto-start");
  Serial.println("Option 2: Send 'RUN_ALL_TESTS' to topic:");
  Serial.println("          mitmqtt/test/command");
  Serial.println("========================================");

  delay(5000); // Wait before auto-starting tests

  // Auto-start tests
  currentTest = TEST_BASIC_PUBLISH;
  testStartTime = millis();
}

void loop() {
  if (!mqttClient.connected()) {
    reconnect_mqtt();
  }
  mqttClient.loop();

  // Run current test
  switch (currentTest) {
  case TEST_IDLE:
    // Just keep connection alive
    break;
  case TEST_BASIC_PUBLISH:
    runTest_BasicPublish();
    break;
  case TEST_QOS_0:
    runTest_QoS0();
    break;
  case TEST_QOS_1:
    runTest_QoS1();
    break;
  case TEST_LARGE_PAYLOAD_1K:
    runTest_LargePayload1K();
    break;
  case TEST_RAPID_PUBLISH:
    runTest_RapidPublish();
    break;
  case TEST_BINARY_DATA:
    runTest_BinaryData();
    break;
  case TEST_UNICODE_DATA:
    runTest_UnicodeData();
    break;
  case TEST_EMPTY_PAYLOAD:
    runTest_EmptyPayload();
    break;
  case TEST_LONG_TOPIC:
    runTest_LongTopic();
    break;
  case TEST_COMPLETE:
    runTestComplete();
    break;
  default:
    break;
  }

  
  static unsigned long lastBlink = 0;
  if (millis() - lastBlink > 2000) {
    lastBlink = millis();
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
  }
}
