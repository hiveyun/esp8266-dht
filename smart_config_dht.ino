// https://github.com/bblanchon/ArduinoJson.git
#include <ArduinoJson.h>
// https://github.com/knolleary/pubsubclient.git
#include <PubSubClient.h>

#include <ESP8266WiFi.h>
#include <WiFiClient.h>

#include <EEPROM.h>
#include <WiFiUDP.h>

#include "dht_fsm_ext.h"
#include "multism.h"

#include "DHT.h"

#define BUTTON 14
#define LED 2

#define DHT_VCC 5
#define DHT_PIN 12
#define DHT_TYPE DHT22

// Initialize DHT sensor.
DHT dht(DHT_PIN, DHT_TYPE);

#define MQTT_USERNAME "a937e135a6881193af39"
#define MQTT_HOST "gw.huabot.com"
#define MQTT_PORT 11883

WiFiClient wifiClient;
PubSubClient client(wifiClient);

WiFiUDP udpServer;

char wifiAP[32];
char wifiPassword[64];

char mqtt_password[40];

unsigned long ledTimer = millis();
unsigned long ledDelay = 1000;

unsigned long buttonPressTimer = millis();
unsigned long smartconfigTimer = millis();
unsigned long sensorTimer = millis();
unsigned long mqttRetryTimer = millis();

float temperature = 0;
float humidity = 0;

bool maybeNeedBind = false;

void setup() {
    EEPROM.begin(512);
    pinMode(BUTTON, INPUT_PULLUP);
    pinMode(LED, OUTPUT);
    pinMode(DHT_VCC, OUTPUT);
    digitalWrite(DHT_VCC, LOW);
    initEventQueue();
    delay(10);
}

void loop() {
  dht_check(NULL);
  flushEventQueue();
  client.loop();
  if (ledTimer + ledDelay < millis()) {
    ledTimer = millis();
    led_toggle(NULL);
  }
}

void initWiFi(void) {
    WiFi.mode(WIFI_STA);

    for (int i = 0; i < 32; ++i) {
      wifiAP[i] = char(EEPROM.read(i));
    }

    for (int i = 32; i < 96; ++i) {
      wifiPassword[i - 32] = char(EEPROM.read(i));
    }

    WiFi.begin(wifiAP, wifiPassword);
}

void initMqtt(void) {
    client.setServer(MQTT_HOST, MQTT_PORT);
    client.setCallback(onMqttMessage);
    for (int i = 96; i < 136; ++i) {
        mqtt_password[i - 96] = char(EEPROM.read(i));
    }
}

void buttonCheck(const button_check_t *a1) {
    if (digitalRead(BUTTON)) {
        button_released(NULL);
    } else {
        button_pressed(NULL);
    }
}

void buttonEnterPressed(void) {
    buttonPressTimer = millis();
}

void buttonPressed(const button_pressed_t *) {
    if (buttonPressTimer + 2000 < millis()) {
        button_longpressed(NULL);
    }
}

void connectCheck(const mqtt_check_t *a1) {
    if(client.connected()) {
        mqtt_connected(NULL);
    } else {
        mqtt_unconnected(NULL);
    }
}

void led1000(void) {
    ledDelay = 1000;
}

void led200(void) {
    ledDelay = 200;
}

void led500(void) {
    ledDelay = 500;
}

void ledOff(void) {
    digitalWrite(LED, HIGH);
}

void ledOn(void) {
    digitalWrite(LED, LOW);
}

void netCheck(const net_check_t *a1) {
    if (WiFi.status() == WL_CONNECTED) {
        net_online(NULL);
    } else {
        net_offline(NULL);
    }
}

void onConnected(void) {
    client.subscribe("/request/+");
}

void beginSmartconfig(void) {
    if (WiFi.status() == WL_CONNECTED) {
        WiFi.disconnect();
    }
    while(WiFi.status() == WL_CONNECTED) {
        delay(100);
    }
    WiFi.beginSmartConfig();
    maybeNeedBind = true;
    smartconfigTimer = millis();
}

void smartconfigDone(const smartconfig_check_t *) {
    if (WiFi.smartConfigDone()) {
        smartconfig_done(NULL);
        strcpy(wifiAP, WiFi.SSID().c_str());
        strcpy(wifiPassword, WiFi.psk().c_str());

        for (int i = 0; i < 32; ++i) {
          EEPROM.write(i, wifiAP[i]);
        }

        for (int i = 32; i < 96; ++i) {
          EEPROM.write(i, wifiPassword[i - 32]);
        }
        EEPROM.commit();
    } else {
        if (millis() - smartconfigTimer > 120000) {
            smartconfig_timeout(NULL);
        }
    }
}

void fetchToken(const mqtt_check_t *) {
    char message = udpServer.parsePacket();
    int packetsize = udpServer.available();
    if (message) {
        char data[200];
        udpServer.read(data,packetsize);

        data[packetsize] = '\0';
        DynamicJsonDocument doc(200);
        deserializeJson(doc, data);
        String type = String((const char*)doc["type"]);
        DynamicJsonDocument rsp(200);

        if (type.equals("Ping")) {
            rsp["type"] = "Pong";
        } else if (type.equals("MqttPass")) {
            strcpy(mqtt_password, doc["value"]);
            rsp["type"] = "Success";
            for (int i = 96; i < 136; ++i) {
              EEPROM.write(i, mqtt_password[i - 96]);
            }
            EEPROM.commit();
            client.disconnect();
            mqtt_done(NULL);
        } else {
            rsp["type"] = "Error";
            rsp["value"] = "Unknow type";
        }

        IPAddress remoteip=udpServer.remoteIP();
        uint16_t remoteport=udpServer.remotePort();
        udpServer.beginPacket(remoteip,remoteport);

        serializeJson(rsp, data);

        udpServer.write(data);

        udpServer.endPacket();
    }
}

void checkPassword(const mqtt_check_t *) {
    if (mqtt_password[0] == '\0') {
        mqtt_invalid(NULL);
    } else {
        mqtt_valid(NULL);
    }
}

void startUdpServer(void) {
    udpServer.begin(1234);
}

void stopUdpServer(void) {
    udpServer.stop();
}

void tryConnect(const mqtt_unconnected_t *) {
    if (mqttRetryTimer + 5000 > millis()) {
         return;
     }

     mqttRetryTimer = millis();
    if (client.connect("ESP8266 Relay", MQTT_USERNAME, mqtt_password)) {
        maybeNeedBind = false;
    } else {
        if (maybeNeedBind) {
            mqtt_failed(NULL);
        }
    }
}

void enterSensorIdle(void) {
    sensorTimer = millis();
}

void sensorIdleCheck(const sensor_check_t *) {
    if (sensorTimer + 10000 < millis()) {
        sensor_report(NULL);
    }
}

void enterSensorReport(void) {
    sensorTimer = millis();
    digitalWrite(DHT_VCC, HIGH);
    pinMode(DHT_PIN, INPUT_PULLUP);
}

void sensorReportCheck(const sensor_check_t *) {
    if (sensorTimer + 1000 < millis()) {
        sensor_report(NULL);
    }
}

void sensorReport(const sensor_report_t *) {
    // Reading temperature or humidity takes about 250 milliseconds!
    float h = dht.readHumidity();
    // Read temperature as Celsius (the default)
    float t = dht.readTemperature();

    // Check if any reads failed and exit early (to try again).
    if (isnan(h) || isnan(t)) {
      return;
    }

    temperature = t;
    humidity = h;

    DynamicJsonDocument data(100);
    data["temperature"] = temperature;
    data["humidity"] = humidity;
    char payload[100];
    serializeJson(data, payload);
    client.publish("/telemetry", payload);
}

void exitSensorReport(void) {
    digitalWrite(DHT_VCC, LOW);
}

// The callback for when a PUBLISH message is received from the server.
void onMqttMessage(const char* topic, byte* payload, unsigned int length) {
    char json[length + 1];
    strncpy(json, (char*)payload, length);
    json[length] = '\0';

    // Decode JSON request
    DynamicJsonDocument data(1024);
    deserializeJson(data, json);

    // Check request method
    String methodName = String((const char*)data["method"]);
    String responseTopic = String(topic);
    responseTopic.replace("request", "response");

    DynamicJsonDocument rsp(100);
    if (methodName.equals("get_dht11_value")) {
        rsp["temperature"] = temperature;
        rsp["humidity"] = humidity;
    } else {
        rsp["err"] = "Not Support";
    }
    char rspData[100];
    serializeJson(rsp, rspData);
    client.publish(responseTopic.c_str(), rspData);
}
