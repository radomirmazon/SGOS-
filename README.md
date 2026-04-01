# StarGate

> 3D model: [Stargate Portal — Gambody](https://www.gambody.com/premium/stargate-portal)

An animated Stargate prop model controlled by an ESP32-S3 microcontroller. Pressing a button triggers the full dialing sequence: the ring rotates, chevrons light up one by one with sound effects, and the gate opens with a wormhole animation. A long press triggers the incoming wormhole sequence.

## Hardware

| Component | Description |
|-----------|-------------|
| **ESP32-S3-DevKitM-1** | Main controller — dual-core 240 MHz, 16 MB flash, Wi-Fi/BT (Arduino framework via PlatformIO) |
| **PCA9685** (×2, I2C 0x40 / 0x41) | 16-channel 12-bit PWM driver — controls 9 chevron LEDs, 18 blue ring LEDs, and 1 white LED |
| **MAX98357A** | I2S Class-D mono amplifier — plays gate sound effects (PCM 16-bit, 16 kHz) |
| **DC motor driver** | Drives the rotating ring via two PWM inputs (left / right) with a soft ramp-up |
| **Button** | Momentary push-button on GPIO46 — short press = dial out, long press = incoming wormhole |

## GPIO Assignment

| GPIO | Signal | Component |
|------|--------|-----------|
| 5 | Motor right | DC motor driver |
| 6 | Motor left | DC motor driver |
| 8 | I2C SDA | PCA9685 ×2 |
| 9 | I2C SCL | PCA9685 ×2 |
| 15 | I2S BCLK | MAX98357A |
| 16 | I2S LRC | MAX98357A |
| 17 | I2S DIN | MAX98357A |
| 18 | MAX98357A SD (shutdown) | HIGH = on |
| 46 | Button input | Active low |
| GAIN | tied to GND | MAX98357A — fixed 9 dB gain |

## Software Architecture

- **AudioPlayer** — non-blocking I2S playback running on Core 0 (FreeRTOS task). Supports gapless transitions, queued playback, and looping.
- **PWMChannel** — thin wrapper around a single PCA9685 channel with fade/blink animation support.
- **Motor** — soft ramp-up (3 s) and ramp-down for the ring rotation motor.
- **DialingUp** — state machine that orchestrates the full dialing and incoming-wormhole sequences.
- **Button** — debounced button with short-click and long-click callbacks.

## Audio Format

Sound effects are stored as C arrays (`uint8_t[]`) compiled into flash:

- Encoding: PCM 16-bit signed, mono, little-endian
- Sample rate: 16 000 Hz

## Build

Requires [PlatformIO](https://platformio.org/). Open the project folder in VS Code with the PlatformIO extension installed, then click **Build** or run:

```sh
pio run
pio run --target upload
```

## Documentation

- [ESP32-S3 Datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf)
- [ESP32-S3-DevKitM-1 User Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/hw-reference/esp32s3/user-guide-devkitm-1.html)
- [PCA9685 Datasheet](https://www.nxp.com/docs/en/data-sheet/PCA9685.pdf)
- [Adafruit PWM Servo Driver Library](https://github.com/adafruit/Adafruit-PWM-Servo-Driver-Library)
- [MAX98357A Datasheet](https://www.analog.com/media/en/technical-documentation/data-sheets/MAX98357A-MAX98357B.pdf)
- [ESP-IDF I2S Driver](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/i2s.html)
- [PlatformIO ESP32 Platform](https://docs.platformio.org/en/latest/platforms/espressif32.html)
