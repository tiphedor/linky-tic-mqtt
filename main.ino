#include <PubSubClient.h>
#include "LinkyHistTIC.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

#include "config.h"

LinkyHistTIC Linky(LINKY_RX, LINKY_TX);
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
char buffer[50];
bool hasNewBase = false, hasNewIInst = false, hasNewPapp = false;
uint32_t base = 0;
uint8_t iinst = 0;
uint16_t papp = 0;
int lastExec = millis();

void setup() {
  // WIFI
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    if (WiFi.status() == WL_CONNECT_FAILED) {
      delay(30000);
      ESP.restart();
    }
  }

  // MQTT
  mqttClient.setServer(mqttServer, mqttPort);
  if (!mqttClient.connect(mqttClientName, mqttUser, mqttPassword)) {
    delay(30000);
    ESP.restart();
  }

  // OTA
  ArduinoOTA.setPort(otaPort);
  ArduinoOTA.setHostname(otaHostname);
  ArduinoOTA.setPassword(otaPassword);
  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";
    });
  ArduinoOTA.begin();
  Linky.Init();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    delay(30000);
    ESP.restart();
  }
  if (!mqttClient.connected()) {
    delay(30000);
    ESP.restart();
  }

  ArduinoOTA.handle();
  Linky.Update();
  mqttClient.loop();


  if (Linky.baseIsNew()) {
    base = Linky.base();
    hasNewBase = true;
  }

  if (Linky.iinstIsNew()) {
    iinst = Linky.iinst();
    hasNewIInst = true;
  }

  if (Linky.pappIsNew()) {
    papp = Linky.papp();
    hasNewPapp = true;
  }

  if (millis() - lastExec >= mqttRefreshRateMillis) {
    // Executed every `mqttRefreshRateMillis`ms
    lastExec = millis();

    if (hasNewBase) {
      sprintf(buffer, "%d", base);
      mqttClient.publish(mqttTopicIndexConso, buffer);
      hasNewBase = false;
    }
    if (hasNewIInst) {
      sprintf(buffer, "%d", iinst);
      mqttClient.publish(mqttTopicIntensiteInstantanee, buffer);
      hasNewIInst = false;
    }
    if (hasNewPapp) {
      sprintf(buffer, "%d", papp);
      mqttClient.publish(mqttTopicPuissanceApparente, buffer);
      hasNewPapp = false;
    }
  } else {
    delay(10);
  }
}
