#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

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

void setup() {
  Serial.begin(115200);
  // Set output mode for all GPIO pins
  while (!Serial) {};

  pinMode(DHT_VCC, OUTPUT);
  digitalWrite(DHT_VCC, LOW);
  pinMode(SMART_CONFIG_LED, OUTPUT);
  digitalWrite(SMART_CONFIG_LED, HIGH);
  pinMode(SMART_CONFIG_BUTTON, INPUT_PULLUP);
  delay(10);
  InitWiFi();
  client.setServer( thingsboardServer, 1883 );
  lastSend = 0;
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
      delay(500);
      smart_blink();
      if (WiFi.smartConfigDone()) {
        break;
      }
    }
  }
  digitalWrite(SMART_CONFIG_LED, HIGH);
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

  if ( millis() - lastSend > 1000 ) { // Update and send only after 1 seconds
    getAndSendTemperatureAndHumidityData();
    lastSend = millis();
  }

  client.loop();
}

void getAndSendTemperatureAndHumidityData() {
  Serial.println("Collecting temperature data.");

  // Reading temperature or humidity takes about 250 milliseconds!
  float h = dht.readHumidity();
  // Read temperature as Celsius (the default)
  float t = dht.readTemperature();

  // Check if any reads failed and exit early (to try again).
  if (isnan(h) || isnan(t)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }

  Serial.print("Humidity: ");
  Serial.print(h);
  Serial.print(" %\t");
  Serial.print("Temperature: ");
  Serial.print(t);
  Serial.print(" *C ");

  String temperature = String(t);
  String humidity = String(h);


  // Just debug messages
  Serial.print( "Sending temperature and humidity : [" );
  Serial.print( temperature ); Serial.print( "," );
  Serial.print( humidity );
  Serial.print( "]   -> " );

  // Prepare a JSON payload string
  String payload = "{";
  payload += "\"temperature\":"; payload += temperature; payload += ",";
  payload += "\"humidity\":"; payload += humidity;
  payload += "}";

  // Send payload
  char attributes[100];
  payload.toCharArray( attributes, 100 );
  client.publish( "v1/devices/me/telemetry", attributes );
  Serial.println( attributes );
}

void InitWiFi() {
  Serial.println("Connecting to AP ...");
  // attempt to connect to WiFi network

  // set for STA mode
  WiFi.mode(WIFI_STA);

  // WiFi.begin(WIFI_AP, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
    smart_blink();
    smart_config();
  }
  Serial.println("Connected to AP");
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    if ( WiFi.status() != WL_CONNECTED) {
      // WiFi.begin(WIFI_AP, WIFI_PASSWORD);
      while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        smart_blink();
        Serial.print(".");
        smart_config();
      }
      Serial.println("Connected to AP");
    }
    Serial.print("Connecting to Thingsboard node ...");
    // Attempt to connect (clientId, username, password)
    if ( client.connect("ESP8266 Device", TOKEN, NULL) ) {
      Serial.println( "[DONE]" );
    } else {
      Serial.print( "[FAILED] [ rc = " );
      Serial.print( client.state() );
      Serial.println( " : retrying in 5 seconds]" );
      // Wait 5 seconds before retrying
      delay( 5000 );
    }
  }
}
