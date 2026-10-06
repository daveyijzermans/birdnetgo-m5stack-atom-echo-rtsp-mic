# CLAUDE.md - AI Development Context

See `README.md` for project documentation, features, architecture, and usage.

## Key Files
- `src/esp32_rtsp_mic_birdnetgo.ino` — Main firmware (dual-core audio, RTSP, I2S)
- `src/WebUI.cpp` — Web interface (HTML/JS/CSS embedded as string literals, API endpoints)
- `src/WebUI.h` — WebUI header
- `platformio.ini` — Build configuration and dependencies

## Critical Rules

### i2sShiftBits MUST be 0
- The capture loop scales the ES8311's 32-bit slots to 16-bit range itself, no bit shifting needed
- This is hardcoded to 0 in the firmware — do not make it configurable
- If it gets set to any other value, all audio becomes zeros (e.g., `180 >> 11 = 0`)
- Web UI shows it as read-only: "0 bits (fixed for ES8311)"

### ES8311 Codec Init Must Precede Every I2S Driver Install
- `setup_i2s_driver()` calls `echoBase.init()` first (configures ES8311 via I2C **and** installs a temporary I2S driver)
- It then immediately calls `i2s_driver_uninstall()` and reinstalls the driver with our custom DMA/buffer settings
- This sequence must be preserved: the ES8311 expects the I2S clock/format it was configured for, so always call `echoBase.init()` with the same `currentSampleRate` before reinstalling the driver
- `echoBase.init()` configures the codec for 32-bit slots with its MCLK taken from BCLK (64 x fs), so the driver must run `I2S_BITS_PER_SAMPLE_32BIT`; 16-bit slots halve BCLK and the ADC then runs at half the sample rate, sending every sample twice
- `echoBase.init()` also sets the mic preamp to its maximum and switches the speaker amplifier on; `applyMicPga()` and `applySpeaker()` run after every init to restore the configured preamp (`mic_pga`, 0-30 dB) and keep the amplifier off (`speaker`)

### The radio's transmit bursts reach the mic
- On the Echo Base a 9.6-16 kHz noise band rises about 10 dB during every WiFi transmit burst. Transmit power, the preamp setting and the speaker amplifier do not change it: it enters at the mic itself
- The 8th-order low-pass (`lp_enable`, `lp_cutoff`, default 8 kHz, off by default) is the remedy; it costs the top of the highest bird song (goldcrest, firecrest)
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
4. Read the `[HB]` serial line (every 30 s): free heap, largest free block, WiFi state and the time since the
   last packet. Known: the firmware can drop off the network after minutes of streaming while it keeps running
   and WiFi reports connected; the heartbeat records what precedes it.

## Build & Deploy
```bash
pio run                      # Build
pio run --target upload      # Upload over USB
pio device monitor -b 115200 # Monitor
```
USB is the update path. The build gets the espota listener (ArduinoOTA, `m5stack-atom-ota` env, `OTA_HOST`,
`OTA_HOST_PORT`) only when `OTA_PASSWORD` is set at build time (`ota_password.py` bakes in its MD5). The
Atom's USB serial fails at 460800 baud; 1.5 Mbaud (the board's `upload_speed`) works.

## Configuration Storage
Settings saved to flash in `audioPrefs` namespace via ESP32 Preferences library.
