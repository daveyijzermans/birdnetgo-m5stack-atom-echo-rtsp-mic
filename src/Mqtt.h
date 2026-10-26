#pragma once
#include <Arduino.h>

// MQTT (only when built with MQTT_HOST): the thermal state on the broker and its clear topic. Call mqtt_begin() once
// the hostname is known, mqtt_loop() from loop().
void mqtt_begin();
void mqtt_loop();
