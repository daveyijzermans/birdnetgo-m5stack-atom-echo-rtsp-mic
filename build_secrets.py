# PlatformIO pre-script: what the build takes from its environment, never from the source.
# OTA_PASSWORD builds network updates (OTA) in; only the password's MD5 enters the firmware.
# MQTT_HOST builds the MQTT client in (Mqtt.cpp), with MQTT_PORT, MQTT_USERNAME and MQTT_PASSWORD.
import hashlib
import os

Import("env")  # noqa: F821 (provided by PlatformIO)

password = os.environ.get("OTA_PASSWORD", "")
if password:
    digest = hashlib.md5(password.encode()).hexdigest()
    env.Append(CPPDEFINES=[("OTA_PASSWORD_HASH", env.StringifyMacro(digest))])
else:
    print("OTA_PASSWORD not set: building without network updates")

host = os.environ.get("MQTT_HOST", "")
if host:
    env.Append(CPPDEFINES=[("MQTT_HOST", env.StringifyMacro(host))])
    for key in ("MQTT_USERNAME", "MQTT_PASSWORD"):
        if os.environ.get(key):
            env.Append(CPPDEFINES=[(key, env.StringifyMacro(os.environ[key]))])
    if os.environ.get("MQTT_PORT"):
        env.Append(CPPDEFINES=[("MQTT_PORT", int(os.environ["MQTT_PORT"]))])
else:
    print("MQTT_HOST not set: building without MQTT")
