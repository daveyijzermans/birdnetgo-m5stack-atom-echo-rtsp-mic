# CLAUDE.md - AI Development Context

See `README.md` for project documentation, features, architecture, and usage.

## Key Files
- `src/esp32_rtsp_mic_birdnetgo.ino` — Main firmware (dual-core audio, RTSP, I2S)
- `src/WebUI.cpp` — Web interface (HTML/JS/CSS embedded as string literals, API endpoints)
- `src/WebUI.h` — WebUI header
- `platformio.ini` — Build configuration and dependencies

## Critical Rules

### i2sShiftBits MUST be 0
- The ES8311 codec on the Atomic Echo Base outputs standard signed 16-bit PCM, no bit shifting needed
- This is hardcoded to 0 in the firmware — do not make it configurable
- If it gets set to any other value, all audio becomes zeros (e.g., `180 >> 11 = 0`)
- Web UI shows it as read-only: "0 bits (fixed for ES8311)"

### ES8311 Codec Init Must Precede Every I2S Driver Install
- `setup_i2s_driver()` calls `echoBase.init()` first (configures ES8311 via I2C **and** installs a temporary I2S driver)
- It then immediately calls `i2s_driver_uninstall()` and reinstalls the driver with our custom DMA/buffer settings
- This sequence must be preserved: the ES8311 expects the I2S clock/format it was configured for, so always call `echoBase.init()` with the same `currentSampleRate` before reinstalling the driver
- Hardware: M5Stack Atom + Atomic Echo Base (A149); pins: I2C SDA=25, SCL=21; I2S BCLK=33, LRCLK=19, DIN=23

### Socket Ownership Model
- Core 1 exclusively owns the WiFiClient socket during streaming
- Core 0 must never touch the socket while streaming is active
- Use `requestStreamStop()` for Core 0 to signal Core 1 to stop — never close the socket from Core 0
- Task shutdown uses a FreeRTOS semaphore with 2s timeout (confirmed exit pattern)

### Cross-Core Safety
- Use FreeRTOS queues for inter-core data transfer (not custom ring buffers — cache coherency issues on ESP32)
- Use `portMUX_TYPE` spinlocks for shared log buffers
- Use Xtensa `memw` memory barriers on critical flag transitions between cores
- `core1OwnsLED` flag prevents concurrent FastLED/RMT driver access

## Debugging Tips

### If Audio Stops Working
1. Check serial monitor for `i2sShiftBits` value — must be 0
2. Uncomment debug lines in `streamAudio()` to see sample values at each processing step
3. Check raw I2S samples first, then processed samples, then RTP packets
4. Use "Defaults" button in Web UI if flash settings are corrupted

### If System is Unstable
1. Check WiFi RSSI (should be > -70 dBm)
2. Check free heap (should stay above ~80KB)
3. Look for "Write timeout" messages (client too slow)

## Build & Deploy
```bash
pio run                      # Build
pio run --target upload      # Upload
pio device monitor -b 115200 # Monitor
```

## Configuration Storage
Settings saved to flash in `audioPrefs` namespace via ESP32 Preferences library.
