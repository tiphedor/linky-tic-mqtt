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

void setup() {
  Serial.begin(115200);
  Serial.println("Booting!");

  // WIFI
  Serial.print("Connecting to Wi-Fi");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(250);

    if (WiFi.status() == WL_CONNECT_FAILED) {
      Serial.println(" WiFi Error!");
      Serial.println("Rebooting...");
      delay(30000);
      ESP.restart();
    }
  }
  Serial.println(" Ok!");

  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // MQTT
  Serial.print("Connecting to MQTT");
  mqttClient.setServer(mqttServer, mqttPort);
  if (mqttClient.connect("esp32-linky", mqttUser, mqttPassword)) {
    Serial.println(" Ok!");
  } else {
    Serial.print("MQTT failed, rc=");
    Serial.print(mqttClient.state());

    Serial.println("Rebooting...");
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

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });
  ArduinoOTA.begin();

  Linky.Init();
}


void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi lost - reboot !");
    delay(30000);
    ESP.restart();
  }

  ArduinoOTA.handle();
  Linky.Update();
  mqttClient.loop();

  if (Linky.baseIsNew()) {
    Serial.print("New value for base:");
    Serial.println(Linky.base());

    sprintf(buffer, "%d", Linky.base());
    mqttClient.publish(mqttTopicIndexConso, buffer);
  }

  if (Linky.iinstIsNew()) {
    Serial.print("New value for Iinst:");
    Serial.println(Linky.iinst());

    sprintf(buffer, "%d", Linky.iinst());
    mqttClient.publish(mqttTopicIntensiteInstantanee, buffer);
  }

  if (Linky.pappIsNew()) {
    Serial.print("New value for Papp:");
    Serial.println(Linky.papp());

    sprintf(buffer, "%d", Linky.papp());
    mqttClient.publish(mqttTopicPuissanceApparente, buffer);
  }

  delay(25);
}
