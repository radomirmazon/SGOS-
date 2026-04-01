# Button

Non-blocking button handler with debounce, short click, and long press detection.

## Files

- `src/Button.h`
- `src/Button.cpp`

## Timing constants

| Constant       | Value | Description                              |
|----------------|-------|------------------------------------------|
| `DEBOUNCE_MS`  | 50 ms | Minimum stable signal time (debounce)    |
| `LONG_PRESS_MS`| 800 ms| Hold time required to trigger long press |

## API

### `begin(pin, activeLow = true)`

Initializes the button. Call once in `setup()`.

| Parameter   | Type      | Default | Description                                      |
|-------------|-----------|---------|--------------------------------------------------|
| `pin`       | `uint8_t` | —       | GPIO pin number                                  |
| `activeLow` | `bool`    | `true`  | `true` = button connects pin to GND (INPUT_PULLUP); `false` = button connects pin to VCC (INPUT_PULLDOWN) |

### `loop()`

State machine tick. Call on every iteration of Arduino `loop()`. Never blocks.

### `onShortClick(callback)`

Registers a callback fired on short click (button released before `LONG_PRESS_MS`).

### `onLongClick(callback)`

Registers a callback fired once after holding the button for `LONG_PRESS_MS`. Does not wait for release.

## Usage

```cpp
#include "Button.h"

Button btn;

void setup() {
    btn.begin(GPIO_NUM_0);          // active low (button to GND)

    btn.onShortClick([]() {
        // short click action
    });

    btn.onLongClick([]() {
        // long press action
    });
}

void loop() {
    btn.loop();
}
```

## Behavior

```
Press ──────────────┐           Short click:
                    └── release  → onShortClick fires on release

Press ──────────────────────┐   Long press (≥ 800 ms):
         800 ms elapsed ↑   │   → onLongClick fires immediately
                            └── release  (no short click fired)
```
