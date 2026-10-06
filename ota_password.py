# PlatformIO pre-script: build network updates (OTA) in when OTA_PASSWORD is set in the environment.
# Only the password's MD5 enters the firmware; without the variable the build has no OTA.
import hashlib
import os

Import("env")  # noqa: F821 (provided by PlatformIO)

password = os.environ.get("OTA_PASSWORD", "")
if password:
    digest = hashlib.md5(password.encode()).hexdigest()
    env.Append(CPPDEFINES=[("OTA_PASSWORD_HASH", env.StringifyMacro(digest))])
else:
    print("OTA_PASSWORD not set: building without network updates")
