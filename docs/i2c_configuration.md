# I2C Configuration

## Pins

| Signal | GPIO |
|--------|------|
| SDA    | 8    |
| SCL    | 9    |

## Devices

| Address | Device   | Role                     |
|---------|----------|--------------------------|
| 0x40    | PCA9685  | Chevron LEDs and servos  |
| 0x41    | PCA9685  | Chevron LEDs and servos  |

## Initialization

`Wire.begin()` jest wywoływany niejawnie wewnątrz `Adafruit_I2CDevice::begin()` — brak jawnego `Wire.begin(8, 9)` w `main.cpp`.

Kod działa poprawnie, jednak konfiguracja jest niejawna. Zalecane jest dodanie jawnego wywołania w `setup()` przed inicjalizacją sterowników:

```cpp
Wire.begin(8, 9);  // SDA=GPIO8, SCL=GPIO9
pca0.begin();
pca1.begin();
```
