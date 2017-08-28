#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <EEPROM.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include "DHT.h"

#define SMART_CONFIG_BUTTON 14
#define SMART_CONFIG_LED 2

#define DHT_VCC 5
#define DHT_PIN 12
#define DHT_TYPE DHT22

// Initialize DHT sensor.
DHT dht(DHT_PIN, DHT_TYPE);

bool blinkStatus = false;

char thingsboardServer[] = "tb.codecard.cn";

WiFiClient wifiClient;

PubSubClient client(wifiClient);

ESP8266WebServer server(80);

unsigned long lastSend;
unsigned long periodic = 15000;

char wifiAP[32];
char wifiPassword[64];
char token[30];

void setup() {
  EEPROM.begin(512);
  pinMode(DHT_VCC, OUTPUT);
  digitalWrite(DHT_VCC, LOW);
  pinMode(SMART_CONFIG_LED, OUTPUT);
  digitalWrite(SMART_CONFIG_LED, HIGH);
  pinMode(SMART_CONFIG_BUTTON, INPUT_PULLUP);
  delay(10);

  for (int i = 0; i < 32; ++i) {
    wifiAP[i] = char(EEPROM.read(i));
  }

  for (int i = 32; i < 96; ++i) {
    wifiPassword[i - 32] = char(EEPROM.read(i));
  }

  for (int i = 96; i < 126; ++i) {
    token[i - 96] = char(EEPROM.read(i));
  }

  InitWiFi();
  client.setServer( thingsboardServer, 1883 );
  client.setCallback(onMessage);

  server.on("/update_token", HTTP_POST, handleSetToken);
  server.onNotFound(handleNotFound);
  server.begin();
}

// The callback for when a PUBLISH message is received from the server.
void onMessage(const char* topic, byte* payload, unsigned int length) {

  char json[length + 1];
  strncpy(json, (char*)payload, length);
  json[length] = '\0';

  // Decode JSON request
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& data = jsonBuffer.parseObject((char*)json);

  if (!data.success()) {
    return;
  }

  // Check request method
  String methodName = String((const char*)data["method"]);

  if (methodName.equals("getPeriodic")) {
    // Reply with GPIO status
    String responseTopic = String(topic);
    responseTopic.replace("request", "response");
    client.publish(responseTopic.c_str(), getPeriodic().c_str());
  } else if (methodName.equals("setPeriodic")) {
    // Update GPIO status and reply
    setPeriodic(data["params"]["periodic"]);
    String responseTopic = String(topic);
    responseTopic.replace("request", "response");
    client.publish(responseTopic.c_str(), getPeriodic().c_str());
    client.publish("v1/devices/me/attributes", getPeriodic().c_str());
  }
}

String getPeriodic() {
  // Prepare gpios JSON payload string
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& data = jsonBuffer.createObject();
  data["periodic"] = periodic;
  char payload[100];
  data.printTo(payload, sizeof(payload));
  return String(payload);
}

void setPeriodic(unsigned long periodic_) {
    if (periodic_ < 2000) {
        periodic_ = 2000;
    }
    periodic = periodic_;
}

String getLocalIP() {
  // Prepare relays JSON payload string
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& data = jsonBuffer.createObject();
  data["ip"] = WiFi.localIP().toString();
  char payload[100];
  data.printTo(payload, sizeof(payload));
  return String(payload);
}

void smartConfig() {
  if (digitalRead(SMART_CONFIG_BUTTON)) {
    return;
  }

  delay(2000);
  if (digitalRead(SMART_CONFIG_BUTTON)) {
    return;
  }
  WiFi.disconnect();

  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    smartBlink();
    WiFi.beginSmartConfig();
    while(1){
      delay(200);
      smartBlink();
      if (WiFi.smartConfigDone()) {
        break;
      }
    }
  }

  closeBlink();

  strcpy(wifiAP, WiFi.SSID().c_str());
  strcpy(wifiPassword, WiFi.psk().c_str());

  for (int i = 0; i < 32; ++i) {
    EEPROM.write(i, wifiAP[i]);
  }

  for (int i = 32; i < 96; ++i) {
    EEPROM.write(i, wifiPassword[i - 32]);
  }
  EEPROM.commit();
}

void smartBlink() {
  if (blinkStatus) {
    closeBlink();
  } else {
    openBlink();
  }
}

void openBlink() {
  digitalWrite(SMART_CONFIG_LED, LOW);
  blinkStatus = true;
}

void closeBlink() {
  digitalWrite(SMART_CONFIG_LED, HIGH);
  blinkStatus = false;
}

void threadDelay(unsigned long timeout) {
  unsigned long lastCheck = millis();
  while (millis() - lastCheck < timeout) {
    if (client.connected()) {
      client.loop();
    }
    smartConfig();
    server.handleClient();
  }
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }

  if (millis() - lastSend > periodic - 1000) { // Update and send only after 1 seconds
    digitalWrite(DHT_VCC, HIGH);
    pinMode(DHT_PIN, INPUT_PULLUP);
    threadDelay(1000);
    getAndSendTemperatureAndHumidityData();
    digitalWrite(DHT_VCC, LOW);
    lastSend = millis();
  }

  client.loop();
  smartConfig();
  server.handleClient();
}

void getAndSendTemperatureAndHumidityData() {
  // Reading temperature or humidity takes about 250 milliseconds!
  float h = dht.readHumidity();
  // Read temperature as Celsius (the default)
  float t = dht.readTemperature();

  // Check if any reads failed and exit early (to try again).
  if (isnan(h) || isnan(t)) {
    return;
  }

  String temperature = String(t);
  String humidity = String(h);

  // Prepare a JSON payload string
  String payload = "{";
  payload += "\"temperature\":"; payload += temperature; payload += ",";
  payload += "\"humidity\":"; payload += humidity;
  payload += "}";

  // Send payload
  char attributes[100];
  payload.toCharArray(attributes, 100);
  client.publish("v1/devices/me/telemetry", attributes);
}

void InitWiFi() {
  // attempt to connect to WiFi network
  // set for STA mode
  WiFi.mode(WIFI_STA);

  WiFi.begin(wifiAP, wifiPassword);
  configWiFiWithSmartConfig();
}

void configWiFiWithSmartConfig() {
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    smartBlink();
    smartConfig();
  }
  closeBlink();
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    if (WiFi.status() != WL_CONNECTED) {
      configWiFiWithSmartConfig();
    }
    // Attempt to connect (clientId, username, password)
    if (client.connect("ESP8266 Relay", token, NULL)) {
      client.subscribe("v1/devices/me/rpc/request/+");
      client.publish("v1/devices/me/attributes", getPeriodic().c_str());
      client.publish("v1/devices/me/attributes", getLocalIP().c_str());
    } else {
      // Wait 5 seconds before retrying

      threadDelay(5000);
      smartBlink();
    }
  }
  closeBlink();
}

void handleSetToken() {
  boolean updated = false;
  for (uint8_t i=0; i<server.args(); i++){
    if (server.argName(i) == "token") {
        strcpy(token, server.arg(i).c_str());
        updated = true;
        break;
    };
  }
  if (updated) {
    for (int i = 96; i < 126; ++i) {
      EEPROM.write(i, token[i - 96]);
    }
    EEPROM.commit();
    client.disconnect();
  }
  server.send(200, "text/plain", "OK");
}

void handleNotFound(){
  server.send(404, "text/plain", "Not Found");
}
