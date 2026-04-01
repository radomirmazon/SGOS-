# Motor — sterownik silnika DC

## Opis

Klasa `Motor` obsługuje silnik DC podłączony do sterownika z **dwoma wejściami PWM**:

| Pin       | Funkcja                                      |
|-----------|----------------------------------------------|
| `rightPin` | PWM obrotu w prawo; `leftPin` musi być LOW  |
| `leftPin`  | PWM obrotu w lewo; `rightPin` musi być LOW  |

> **Ograniczenie sprzętowe:** oba wejścia w stanie HIGH jednocześnie są zabronione. Implementacja wymusza bezpieczną kolejność: najpierw zeruje nieaktywny pin, potem rampuje aktywny.

## Pliki

- `src/Motor.h` — deklaracja klasy, stała `MOTOR_RAMP_MS`
- `src/Motor.cpp` — implementacja

## Interfejs publiczny

```cpp
Motor(uint8_t rightPin, uint8_t leftPin);
void startLeft();
void startRight();
void stop();
```

### Konstruktor

Konfiguruje oba piny jako wyjścia i ustawia je na 0 (`analogWrite`).

### `startLeft()`

1. Natychmiast zeruje `rightPin` (bezpieczeństwo).
2. Rampuje `leftPin` od 0 do 255 (miękki start w lewo).

### `startRight()`

1. Natychmiast zeruje `leftPin` (bezpieczeństwo).
2. Rampuje `rightPin` od 0 do 255 (miękki start w prawo).

### `stop()`

Natychmiast ustawia oba piny na 0. Brak rampy hamowania.

## Miękki start (rampa PWM)

Prywatna metoda `_rampUp(uint8_t drivePin)` realizuje blokującą rampę:

```
duty: 0 → 255, krok co ~2 ms  (łącznie ≈ 500 ms)
```

Czas rampy konfigurowany stałą w `Motor.h`:

```cpp
static constexpr uint16_t MOTOR_RAMP_MS = 500;
```

Zmiana wartości przekłada się automatycznie na opóźnienie między krokami (`MOTOR_RAMP_MS / 255`).

## Podłączenie (ESP32-S3)

| GPIO | Konstruktor  | Wejście sterownika         |
|------|-------------|----------------------------|
| GPIO5 | `rightPin` | IN prawo (np. IN_R)       |
| GPIO6 | `leftPin`  | IN lewo (np. IN_L)        |
| GND  | —           | GND sterownika (masa wspólna) |

> Oba wejścia nigdy nie mogą być HIGH jednocześnie — sprzętowe ograniczenie sterownika.

## Zależności

- Arduino framework (`analogWrite`, `pinMode`, `delay`)
- ESP32-S3: `analogWrite` dostępne od arduino-esp32 ≥ 2.x
