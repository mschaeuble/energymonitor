#define PxMATRIX_double_buffer true

#include <PxMatrix.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Ticker.h>
#include <ArduinoOTA.h>
#include "arduino_secrets.h"

// === CONFIGURATION ====
const char* ssid = SECRET_WIFI_SSID;
const char* password = SECRET_WIFI_PASSWORD;
const char* mqtt_server = SECRET_MQTT_SERVER;
const char* mqtt_username = SECRET_MQTT_USER;
const char* mqtt_password = SECRET_MQTT_PASSWORD;
const char* mqtt_client_id = "Energy-Monitor";
const uint16_t mqtt_port = 1883;

const char* pv_power_topic = "homeassistant/pv-power";
const char* grid_power_topic = "homeassistant/grid-power";
const char* power_consumption_topic = "homeassistant/power-consumption";
const char* battery_soc_topic = "homeassistant/battery-soc";
// =======================

WiFiClient espClient;
PubSubClient mqttClient(espClient);
Ticker displayTicker;

bool stateChanged = false;
unsigned long lastDisplayUpdate = 0;
const unsigned long displayUpdateInterval = 250;  // ms

unsigned long lastMqttReconnectAttempt = 0;
const unsigned long mqttReconnectInterval = 5000;  // ms

// This defines the 'on' time of the display is us. The larger this number,
// the brighter the display. If too large the ESP will crash
const uint8_t display_draw_time = 50;  //30-70 is usually fine

#define MATRIX_WIDTH 64
#define MATRIX_HEIGHT 32

PxMATRIX display(MATRIX_WIDTH, MATRIX_HEIGHT, 16, 2, 5, 4, 15, 12);

// colors
const uint16_t white = display.color565(255, 255, 255);
const uint16_t red = display.color565(255, 77, 77);
const uint16_t orange = display.color565(255, 196, 128);
const uint16_t green = display.color565(159, 255, 128);
const uint16_t blue = display.color565(0, 0, 255);
const uint16_t black = display.color565(0, 0, 0);

// Converted using the following site: http://www.rinkydinkelectronics.com/t_imageconverter565.php
const uint16_t sun[] PROGMEM = {
  0xDE03, 0x0000, 0x0000, 0x0000, 0xE623, 0x0000, 0x0000, 0x0000, 0xCD63, 0x0000, 0xDE03, 0x0000, 0x0000, 0xEE63, 0x0000, 0x0000,  // 0x0010 (16) pixels
  0xDE03, 0x0000, 0x0000, 0x0000, 0x8BC1, 0xDE03, 0xFEE4, 0xDE03, 0x8BC1, 0x0000, 0x0000, 0x0000, 0x0000, 0xDE03, 0xFEE4, 0xFEE4,  // 0x0020 (32) pixels
  0xFEE4, 0xDE03, 0x0000, 0x0000, 0xE623, 0xF6A3, 0xFEE4, 0xFEE4, 0xFEE4, 0xFEE4, 0xFEE4, 0xF683, 0xD5E3, 0x0000, 0x0000, 0xDE03,  // 0x0030 (48) pixels
  0xFEE4, 0xFEE4, 0xFEE4, 0xDE03, 0x0000, 0x0000, 0x0000, 0x0000, 0x8BC1, 0xDE03, 0xFEE4, 0xDE03, 0x8BC1, 0x0000, 0x0000, 0x0000,  // 0x0040 (64) pixels
  0xDE03, 0x0000, 0x0000, 0xDE03, 0x0000, 0x0000, 0xF6A3, 0x0000, 0xDE03, 0x0000, 0x0000, 0x0000, 0xDE03, 0x0000, 0x0000, 0x0000,  // 0x0050 (80) pixels
  0xDE03
};

const uint16_t house[] PROGMEM = {
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xE924, 0x0000, 0x0000,  // 0x0010 (16) pixels
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xE924, 0xFFFF, 0xE924, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xE924, 0xD0E3, 0xE924,  // 0x0020 (32) pixels
  0xA8A2, 0xE924, 0x0000, 0x0000, 0xE924, 0xE924, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xD0E3, 0xD0E3, 0x0000, 0xE924, 0xFFFF,  // 0x0030 (48) pixels
  0xD3A0, 0xFFFF, 0x32D6, 0xEF7E, 0xE924, 0x0000, 0x0000, 0xE924, 0xFFFF, 0xD3A0, 0xFFFF, 0xC65C, 0xFFFF, 0xE924, 0x0000, 0x0000,  // 0x0040 (64) pixels
  0xE924, 0xFFFF, 0xD3A0, 0xFFFF, 0xFFFF, 0xFFFF, 0xE924, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,  // 0x0050 (80) pixels
  0x0000
};

const uint16_t pole[] PROGMEM = {
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
  float batterySOC;
};

State state = { 0.0, 0.0, 0.0, 0.0 };

void setup() {
  Serial.begin(115200);

  // 1/16 scan display
  display.begin(16);

  display.clearDisplay();
  displayTicker.attach(0.004, displayUpdater);

  delay(10);
  executeScreenCheck();
  setupWifi();
  setupOTA();

  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setCallback(mqttCallback);
}

void setupOTA() {
  ArduinoOTA.setHostname("energymonitor");
  ArduinoOTA.setPassword(SECRET_OTA_PASSWORD);

  // Detach the refresh ISR so it cannot fire while ArduinoOTA writes flash
  // (which would run flash-resident code with the cache off and crash the ESP).
  // The panel simply stays dark for the update.
  ArduinoOTA.onStart([]() {
    displayTicker.detach();
  });

  ArduinoOTA.onError([](ota_error_t error) {
    ESP.restart();
  });

  ArduinoOTA.onEnd([]() {
    ESP.restart();
  });

  ArduinoOTA.begin();
}

// ISR (interrupt service routine) for display refresh
void displayUpdater() {
  display.display(display_draw_time);
}

void executeScreenCheck() {
  display.fillRect(0, 0, MATRIX_WIDTH, MATRIX_HEIGHT, display.color565(255, 0, 0));
  display.showBuffer();
  delay(1000);
  display.fillRect(0, 0, MATRIX_WIDTH, MATRIX_HEIGHT, display.color565(0, 255, 0));
  display.showBuffer();
  delay(1000);
  display.fillRect(0, 0, MATRIX_WIDTH, MATRIX_HEIGHT, display.color565(0, 0, 255));
  display.showBuffer();
  delay(1000);
  display.clearDisplay();
  display.showBuffer();
}

void setupWifi() {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextColor(blue);
  display.printf("Connecting to %s", ssid);
  display.showBuffer();

  WiFi.mode(WIFI_STA);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextColor(blue);
  display.println("connected");
  display.println(WiFi.localIP());
  display.showBuffer();

  delay(2000);
}

void loop() {
  if (!mqttClient.connected()) {
    if (millis() - lastMqttReconnectAttempt >= mqttReconnectInterval) {
      lastMqttReconnectAttempt = millis();
      connectToMqttServer();
    }
  } else {
    mqttClient.loop();
  }
  ArduinoOTA.handle();

  if (stateChanged && millis() - lastDisplayUpdate >= displayUpdateInterval) {
    stateChanged = false;
    lastDisplayUpdate = millis();
    updateDisplay();
  }
}

void connectToMqttServer() {
  Serial.print("Attempting MQTT connection...");

  if (mqttClient.connect(mqtt_client_id, mqtt_username, mqtt_password)) {
    Serial.println("connected");

    mqttClient.subscribe(pv_power_topic);
    mqttClient.subscribe(grid_power_topic);
    mqttClient.subscribe(power_consumption_topic);
    mqttClient.subscribe(battery_soc_topic);
  } else {
    Serial.print("failed, rc=");
    Serial.print(mqttClient.state());
    Serial.println(" trying again in 5 seconds");
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  // PubSubClient's payload is not null-terminated and its buffer is reused,
  // so copy exactly `length` bytes out and terminate it ourselves before
  // parsing — reading past `length` would pick up stale bytes from a
  // previous (longer) message.
  char buf[16];
  size_t len = min(length, sizeof(buf) - 1);
  memcpy(buf, payload, len);
  buf[len] = '\0';
  float floatPayload = atof(buf);

  Serial.printf("Message arrived [%s]: %.1f\n", topic, floatPayload);

  if (strcmp(topic, pv_power_topic) == 0) {
    state.pvPower = floatPayload;
  } else if (strcmp(topic, grid_power_topic) == 0) {
    state.gridPower = floatPayload;
  } else if (strcmp(topic, power_consumption_topic) == 0) {
    state.powerConsumption = floatPayload;
  } else if (strcmp(topic, battery_soc_topic) == 0) {
    state.batterySOC = floatPayload;
  }

  stateChanged = true;
}

void updateDisplay() {
  display.clearDisplay();

  drawBattery();

  display.setCursor(12, 2);
  display.setTextColor(white);
  display.printf("%5.0f", state.pvPower);
  display.setCursor(45, 2);
  display.printf("W");

  display.setCursor(12, 12);
  display.setTextColor(white);
  display.printf("%5.0f", state.powerConsumption);
  display.setCursor(45, 12);
  display.printf("W");

  display.setCursor(12, 22);
  if (state.gridPower > 0) {
    display.setTextColor(red);
  } else if (state.gridPower < 0) {
    display.setTextColor(green);
  } else {
    display.setTextColor(white);
  }
  display.printf("%5.0f", state.gridPower);
  display.setCursor(45, 22);
  display.printf("W");

  drawIcons();

  display.showBuffer();
}

// Battery indicator geometry (left edge of the panel).
const int BAT_X = 1;
const int BAT_Y = 1;
const int BAT_BODY_W = 9;
const int BAT_BODY_H = 28;
const int BAT_BODY_RADIUS = 2;
const int BAT_BODY_Y_OFFSET = 2;            // body top, below the tip
const int BAT_TIP_W = 3;
const int BAT_TIP_H = 3;
const int BAT_TIP_X_OFFSET = 3;
const int BAT_FILL_INSET = 1;               // gap between body and fill
const int BAT_FILL_W = BAT_BODY_W - 2 * BAT_FILL_INSET;
const int BAT_FILL_MAX_H = 26;              // fillable inner height
const int BAT_FILL_TOP = BAT_Y + BAT_BODY_Y_OFFSET + BAT_FILL_INSET;
const float BAT_SOC_LOW = 20.0;             // <= red
const float BAT_SOC_HIGH = 50.0;            // > green, else orange

void drawBattery() {
  // battery tip
  display.fillRect(BAT_X + BAT_TIP_X_OFFSET, BAT_Y, BAT_TIP_W, BAT_TIP_H, white);

  // battery body/hull
  display.drawRoundRect(BAT_X, BAT_Y + BAT_BODY_Y_OFFSET, BAT_BODY_W, BAT_BODY_H, BAT_BODY_RADIUS, white);

  int batteryFillHeight = round(state.batterySOC / 100.0 * BAT_FILL_MAX_H);
  if (batteryFillHeight > 0) {
    uint16_t fillColor;
    if (state.batterySOC <= BAT_SOC_LOW) {
      fillColor = red;
    } else if (state.batterySOC > BAT_SOC_HIGH) {
      fillColor = green;
    } else {
      fillColor = orange;
    }

    display.fillRect(BAT_X + BAT_FILL_INSET, BAT_FILL_TOP + BAT_FILL_MAX_H - batteryFillHeight, BAT_FILL_W, batteryFillHeight, fillColor);
  }
}

const int ICON_SIZE = 9;
const int ICON_X = 54;

void drawIcons() {
  display.drawRGBBitmap(ICON_X, 1, sun, ICON_SIZE, ICON_SIZE);
  display.drawRGBBitmap(ICON_X, 11, house, ICON_SIZE, ICON_SIZE);
  display.drawRGBBitmap(ICON_X, 21, pole, ICON_SIZE, ICON_SIZE);
}
