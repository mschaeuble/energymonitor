#include <PxMatrix.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Ticker.h>
#include "arduino_secrets.h"

// === CONFIGURATION ====
const char* ssid = SECRET_WIFI_SSID;
const char* password = SECRET_WIFI_PASSWORD;
const char* mqtt_server = SECRET_MQTT_SERVER;
const char* mqtt_username = SECRET_MQTT_USER;
const char* mqtt_password = SECRET_MQTT_PASSWORD;
const char* mqtt_client_id = "Energy-Montor";
const uint16_t mqtt_port = 1883;

const char* pv_power_topic = "homeassistant/pv-power";
const char* grid_power_topic = "homeassistant/grid-power";

// =======================

WiFiClient espClient;
PubSubClient mqttClient(espClient);
Ticker displayTicker;

// This defines the 'on' time of the display is us. The larger this number,
// the brighter the display. If too large the ESP will crash
uint8_t display_draw_time = 60;  //30-70 is usually fine

#define MATRIX_WIDTH 64
#define MATRIX_HEIGHT 32

PxMATRIX display(MATRIX_WIDTH, MATRIX_HEIGHT, 16, 2, 5, 4, 15, 12);

// colors
const uint16_t white = display.color565(255, 255, 255);
const uint16_t red = display.color565(255, 77, 77);
const uint16_t green = display.color565(159, 255, 128);
const uint16_t blue = display.color565(0, 0, 255);
const uint16_t black = display.color565(0, 0, 0);

// Converted using the following site: http://www.rinkydinkelectronics.com/t_imageconverter565.php
uint16_t static sun[] = {
  0xDE03, 0x0000, 0x0000, 0x0000, 0xE623, 0x0000, 0x0000, 0x0000, 0xCD63, 0x0000, 0xDE03, 0x0000, 0x0000, 0xEE63, 0x0000, 0x0000,  // 0x0010 (16) pixels
  0xDE03, 0x0000, 0x0000, 0x0000, 0x8BC1, 0xDE03, 0xFEE4, 0xDE03, 0x8BC1, 0x0000, 0x0000, 0x0000, 0x0000, 0xDE03, 0xFEE4, 0xFEE4,  // 0x0020 (32) pixels
  0xFEE4, 0xDE03, 0x0000, 0x0000, 0xE623, 0xF6A3, 0xFEE4, 0xFEE4, 0xFEE4, 0xFEE4, 0xFEE4, 0xF683, 0xD5E3, 0x0000, 0x0000, 0xDE03,  // 0x0030 (48) pixels
  0xFEE4, 0xFEE4, 0xFEE4, 0xDE03, 0x0000, 0x0000, 0x0000, 0x0000, 0x8BC1, 0xDE03, 0xFEE4, 0xDE03, 0x8BC1, 0x0000, 0x0000, 0x0000,  // 0x0040 (64) pixels
  0xDE03, 0x0000, 0x0000, 0xDE03, 0x0000, 0x0000, 0xF6A3, 0x0000, 0xDE03, 0x0000, 0x0000, 0x0000, 0xDE03, 0x0000, 0x0000, 0x0000,  // 0x0050 (80) pixels
  0xDE03
};

uint16_t static house[] = {
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xE924, 0x0000, 0x0000,  // 0x0010 (16) pixels
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xE924, 0xFFFF, 0xE924, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xE924, 0xD0E3, 0xE924,  // 0x0020 (32) pixels
  0xA8A2, 0xE924, 0x0000, 0x0000, 0xE924, 0xE924, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xD0E3, 0xD0E3, 0x0000, 0xE924, 0xFFFF,  // 0x0030 (48) pixels
  0xD3A0, 0xFFFF, 0x32D6, 0xEF7E, 0xE924, 0x0000, 0x0000, 0xE924, 0xFFFF, 0xD3A0, 0xFFFF, 0xC65C, 0xFFFF, 0xE924, 0x0000, 0x0000,  // 0x0040 (64) pixels
  0xE924, 0xFFFF, 0xD3A0, 0xFFFF, 0xFFFF, 0xFFFF, 0xE924, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,  // 0x0050 (80) pixels
  0x0000
};

uint16_t static pole[] = {
  0x0000, 0x0000, 0x096A, 0x1B14, 0x1B56, 0x1B14, 0x096A, 0x0000, 0x0000, 0x0000, 0x096A, 0x1B14, 0x0000, 0x1B56, 0x0000, 0x1B14,  // 0x0010 (16) pixels
  0x096A, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x1B56, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x096A, 0x1B14, 0x1B56,  // 0x0020 (32) pixels
  0x1B14, 0x09ED, 0x0043, 0x0000, 0x0000, 0x096A, 0x1B14, 0x0000, 0x1B56, 0x0000, 0x12B2, 0x096A, 0x0000, 0x0000, 0x0000, 0x0000,  // 0x0030 (48) pixels
  0x0000, 0x1B56, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x1B56, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,  // 0x0040 (64) pixels
  0x0000, 0x0000, 0x0000, 0x1B56, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x8B24, 0x8B24, 0x8B24, 0x8B24, 0x8B24, 0x8B24, 0x8B24,  // 0x0050 (80) pixels
  0x0000
};

struct State {
  float pvPower;
  float gridPower;
  float powerConsumption;
};

State state = { 0.0, 0.0, 0.0 };

void setup() {
  Serial.begin(9600);

  // 1/16 scan display
  display.begin(16);

  display.clearDisplay();
  displayTicker.attach(0.004, displayUpdater);

  setupWifi();

  display.clearDisplay();
  drawStaticImages();

  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setCallback(mqttCallback);
}

// ISR (interrupt service routine) for display refresh
void displayUpdater() {
  display.display(display_draw_time);
}

void setupWifi() {
  delay(10);

  display.setCursor(0, 0);
  display.setTextColor(blue);
  display.printf("Connecting to %s", ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(3000);
    display.print(".");
  }

  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("connected");
  display.println(WiFi.localIP());

  delay(2000);
}

void loop() {
  if (!mqttClient.connected()) {
    connectToMqttServer();
  }
  mqttClient.loop();
}

void connectToMqttServer() {
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");

    mqttClient.setServer(mqtt_server, 1883);
    mqttClient.setCallback(mqttCallback);

    // Attempt to connect
    if (mqttClient.connect(mqtt_client_id, mqtt_username, mqtt_password)) {
      Serial.println("connected");

      mqttClient.subscribe(pv_power_topic);
      mqttClient.subscribe(grid_power_topic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" trying again in 5 seconds");
      delay(5000);
    }
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  payload[length] = '\0';
  String payloadStr = String((char*)payload);

  Serial.printf("Message arrived [%s]: %s\n", topic, payloadStr);

  float floatPayload = payloadStr.toFloat();

  if (strcmp(topic, pv_power_topic) == 0) {
    state.pvPower = floatPayload;
  } else if (strcmp(topic, grid_power_topic) == 0) {
    state.gridPower = floatPayload;
  }

  state.powerConsumption = state.gridPower + state.pvPower;

  updateDisplay();
}

void drawStaticImages() {
  drawIcon(sun, 1, 1);
  drawIcon(house, 1, 11);
  drawIcon(pole, 1, 21);
}

void drawIcon(uint16_t* icon, int x, int y) {
  int iconHeight = 9;
  int iconWidth = 9;
  int counter = 0;
  for (int yy = 0; yy < iconHeight; yy++) {
    for (int xx = 0; xx < iconWidth; xx++) {
      display.drawPixel(x + xx, y + yy, icon[counter]);
      counter++;
    }
  }
}

void updateDisplay() {
  display.fillRect(10, 2, MATRIX_WIDTH - 10, MATRIX_HEIGHT - 5, black);

  display.setCursor(9, 2);
  display.setTextColor(white);
  display.printf("%7.1f W", state.pvPower);

  display.setCursor(9, 12);
  display.setTextColor(white);
  display.printf("%7.1f W", state.powerConsumption);

  display.setCursor(9, 22);

  if (state.gridPower > 0) {
    display.setTextColor(red);
  } else if (state.gridPower < 0) {
    display.setTextColor(green);
  } else {
    display.setTextColor(white);
  }
  display.printf("%7.1f W", state.gridPower);
}
