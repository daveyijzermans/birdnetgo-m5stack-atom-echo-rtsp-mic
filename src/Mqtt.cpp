// MQTT: the thermal state on the broker, and the latch cleared from it. Built in only when MQTT_HOST is set at build
// time (build_secrets.py); the login comes from the build environment too, never from the source.
//
// Topics, under the hostname: <host>/status (online/offline, retained, the last will), <host>/thermal (the same JSON as
// /api/thermal, retained: at once when the latch, the lockout or the sensor fault changes, else every minute), and
// <host>/thermal/clear (any payload clears a persisted latch, as the web UI's button does).
//
// Runs on Core 0 from loop() with its own socket; the RTSP socket stays Core 1's. A lost broker is retried every
// 10 s without blocking the loop.
#include "Mqtt.h"

#ifdef MQTT_HOST
#include <WiFi.h>
#include <PubSubClient.h>
#include "WebUI.h"

extern String currentHostname;
extern bool overheatLatched;
extern bool overheatLockoutActive;
extern bool overheatSensorFault;

#ifndef MQTT_PORT
#define MQTT_PORT 1883
#endif
#ifndef MQTT_USERNAME
#define MQTT_USERNAME ""
#endif
#ifndef MQTT_PASSWORD
#define MQTT_PASSWORD ""
#endif

static WiFiClient mqttSocket;
static PubSubClient mqtt(mqttSocket);
static String topicStatus, topicThermal, topicClear;
static unsigned long lastAttempt = 0, lastThermal = 0;
static int lastFlags = -1;
static bool clearRequested = false;

static int thermalFlags() {
    return (overheatLatched ? 1 : 0) | (overheatLockoutActive ? 2 : 0) | (overheatSensorFault ? 4 : 0);
}

static void onMessage(char* topic, byte*, unsigned int) {
    // handled in mqtt_loop, outside the client's callback
    if (topicClear == topic) clearRequested = true;
}

static void publishThermal() {
    mqtt.publish(topicThermal.c_str(), thermalJson().c_str(), true);
    lastThermal = millis();
    lastFlags = thermalFlags();
}

void mqtt_begin() {
    topicStatus = currentHostname + "/status";
    topicThermal = currentHostname + "/thermal";
    topicClear = currentHostname + "/thermal/clear";
    mqtt.setServer(MQTT_HOST, MQTT_PORT);
    mqtt.setBufferSize(1024);  // the thermal JSON runs past the default 256 bytes
    mqtt.setSocketTimeout(2);  // a dead broker must not hold the loop
    mqtt.setCallback(onMessage);
}

void mqtt_loop() {
    if (WiFi.status() != WL_CONNECTED) return;
    if (!mqtt.connected()) {
        if (millis() - lastAttempt < 10000 && lastAttempt != 0) return;
        lastAttempt = millis();
        String clientId = currentHostname + "-" + String((uint32_t)ESP.getEfuseMac(), HEX);
        if (!mqtt.connect(clientId.c_str(), MQTT_USERNAME, MQTT_PASSWORD, topicStatus.c_str(), 0, true, "offline")) {
            webui_pushLog("MQTT: connect failed, state " + String(mqtt.state()));
            return;
        }
        mqtt.publish(topicStatus.c_str(), "online", true);
        mqtt.subscribe(topicClear.c_str());
        webui_pushLog(F("MQTT: connected"));
        publishThermal();
    }
    mqtt.loop();
    if (clearRequested) {
        clearRequested = false;
        clearThermalLatch("MQTT");
        publishThermal();
    }
    if (thermalFlags() != lastFlags || millis() - lastThermal > 60000) publishThermal();
}
#else
void mqtt_begin() {}
void mqtt_loop() {}
#endif
