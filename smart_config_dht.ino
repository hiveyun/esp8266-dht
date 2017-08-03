#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <EEPROM.h>

#include "DHT.h"

// #define WIFI_AP "WX_Urwork_2.4G"
// #define WIFI_PASSWORD "urwork2015"

#define TOKEN "GQ7Pn6Vd5SGGjtXGJOA4"

#define SMART_CONFIG_BUTTON 14
#define SMART_CONFIG_LED 2

#define DHT_VCC 5
#define DHT_PIN 12
#define DHT_TYPE DHT22

// Initialize DHT sensor.
DHT dht(DHT_PIN, DHT_TYPE);

bool led_status = false;

char thingsboardServer[] = "tb.codecard.cn";

WiFiClient wifiClient;

PubSubClient client(wifiClient);

unsigned long lastSend;
unsigned long periodic = 15000;

char wifi_ap[32];
char wifi_password[64];

void setup() {
  // Serial.begin(115200);
  // Set output mode for all GPIO pins
  // while (!Serial) {};

  EEPROM.begin(512);
  pinMode(DHT_VCC, OUTPUT);
  digitalWrite(DHT_VCC, LOW);
  pinMode(SMART_CONFIG_LED, OUTPUT);
  digitalWrite(SMART_CONFIG_LED, HIGH);
  pinMode(SMART_CONFIG_BUTTON, INPUT_PULLUP);
  delay(10);

  for (int i = 0; i < 32; ++i) {
    wifi_ap[i] = char(EEPROM.read(i));
  }

  for (int i = 32; i < 96; ++i) {
    wifi_password[i - 32] = char(EEPROM.read(i));
  }

  InitWiFi();
  client.setServer( thingsboardServer, 1883 );
  client.setCallback(on_message);
  lastSend = 0;
}

// The callback for when a PUBLISH message is received from the server.
void on_message(const char* topic, byte* payload, unsigned int length) {

  // Serial.println("On message");

  char json[length + 1];
  strncpy (json, (char*)payload, length);
  json[length] = '\0';

  // Serial.print("Topic: ");
  // Serial.println(topic);
  // Serial.print("Message: ");
  // Serial.println(json);

  // Decode JSON request
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& data = jsonBuffer.parseObject((char*)json);

  if (!data.success())
  {
    // Serial.println("parseObject() failed");
    return;
  }

  // Check request method
  String methodName = String((const char*)data["method"]);

  if (methodName.equals("getPeriodic")) {
    // Reply with GPIO status
    String responseTopic = String(topic);
    responseTopic.replace("request", "response");
    client.publish(responseTopic.c_str(), get_periodic().c_str());
  } else if (methodName.equals("setPeriodic")) {
    // Update GPIO status and reply
    set_periodic(data["params"]["periodic"]);
    String responseTopic = String(topic);
    responseTopic.replace("request", "response");
    client.publish(responseTopic.c_str(), get_periodic().c_str());
    client.publish("v1/devices/me/attributes", get_periodic().c_str());
  }
}

String get_periodic() {
  // Prepare gpios JSON payload string
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& data = jsonBuffer.createObject();
  data["periodic"] = periodic;
  char payload[256];
  data.printTo(payload, sizeof(payload));
  String strPayload = String(payload);
  // Serial.print("Get gpio status: ");
  // Serial.println(strPayload);
  return strPayload;
}

void set_periodic(unsigned long periodic_) {
    if (periodic_ < 2000) {
        periodic_ = 2000;
    }
    periodic = periodic_;
}

void smart_config() {
  if (digitalRead(SMART_CONFIG_BUTTON)) {
    return;
  }

  delay(2000);
  if (digitalRead(SMART_CONFIG_BUTTON)) {
    return;
  }
  WiFi.disconnect();

  bool led_status = true;
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    smart_blink();
    WiFi.beginSmartConfig();
    while(1){
      delay(200);
      smart_blink();
      if (WiFi.smartConfigDone()) {
        break;
      }
    }
  }

  digitalWrite(SMART_CONFIG_LED, HIGH);

  strcpy(wifi_ap, WiFi.SSID().c_str());
  strcpy(wifi_password, WiFi.psk().c_str());

  for (int i = 0; i < 32; ++i) {
    EEPROM.write(i, wifi_ap[i]);
  }

  for (int i = 32; i < 96; ++i) {
    EEPROM.write(i, wifi_password[i - 32]);
  }
  EEPROM.commit();
}

void smart_blink() {
  if (led_status) {
    digitalWrite(SMART_CONFIG_LED, HIGH);
    led_status = false;
  } else {
    digitalWrite(SMART_CONFIG_LED, LOW);
    led_status = true;
  }
}

void loop() {
  if ( !client.connected() ) {
    reconnect();
  }

  if ( millis() - lastSend > periodic - 1000 ) { // Update and send only after 1 seconds
    digitalWrite(DHT_VCC, HIGH);
    pinMode(DHT_PIN, INPUT_PULLUP);
    delay(1000);
    getAndSendTemperatureAndHumidityData();
    digitalWrite(DHT_VCC, LOW);
    lastSend = millis();
  }

  smart_config();

  client.loop();
}

void getAndSendTemperatureAndHumidityData() {
  // Serial.println("Collecting temperature data.");

  // Reading temperature or humidity takes about 250 milliseconds!
  float h = dht.readHumidity();
  // Read temperature as Celsius (the default)
  float t = dht.readTemperature();

  // Check if any reads failed and exit early (to try again).
  if (isnan(h) || isnan(t)) {
    // Serial.println("Failed to read from DHT sensor!");
    return;
  }

  // Serial.print("Humidity: ");
  // Serial.print(h);
  // Serial.print(" %\t");
  // Serial.print("Temperature: ");
  // Serial.print(t);
  // Serial.print(" *C ");

  String temperature = String(t);
  String humidity = String(h);


  // Just debug messages
  // Serial.print( "Sending temperature and humidity : [" );
  // Serial.print( temperature ); Serial.print( "," );
  // Serial.print( humidity );
  // Serial.print( "]   -> " );

  // Prepare a JSON payload string
  String payload = "{";
  payload += "\"temperature\":"; payload += temperature; payload += ",";
  payload += "\"humidity\":"; payload += humidity;
  payload += "}";

  // Send payload
  char attributes[100];
  payload.toCharArray( attributes, 100 );
  client.publish( "v1/devices/me/telemetry", attributes );
  // Serial.println( attributes );
}

void InitWiFi() {
  // Serial.println("Connecting to AP ...");
  // attempt to connect to WiFi network

  // set for STA mode
  WiFi.mode(WIFI_STA);

  WiFi.begin(wifi_ap, wifi_password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    // Serial.print(".");
    smart_blink();
    smart_config();
  }
  // Serial.println("Connected to AP");
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    if ( WiFi.status() != WL_CONNECTED) {
      WiFi.begin(wifi_ap, wifi_password);
      while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        smart_blink();
        // Serial.print(".");
        smart_config();
      }
      // Serial.println("Connected to AP");
    }
    // Serial.print("Connecting to Thingsboard node ...");
    // Attempt to connect (clientId, username, password)
    if ( client.connect("ESP8266 Device", TOKEN, NULL) ) {
      // Serial.println( "[DONE]" );
      client.subscribe("v1/devices/me/rpc/request/+");
      client.publish("v1/devices/me/attributes", get_periodic().c_str());
    } else {
      // Serial.print( "[FAILED] [ rc = " );
      // Serial.print( client.state() );
      // Serial.println( " : retrying in 5 seconds]" );
      // Wait 5 seconds before retrying
      delay( 5000 );
    }
  }
}
