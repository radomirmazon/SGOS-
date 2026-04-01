#pragma once
#include <Arduino.h>

// Czas rampy od 0 do 100% [ms]
static constexpr uint16_t MOTOR_RAMP_MS = 3000;

class Motor {
public:
  Motor(uint8_t rightPin, uint8_t leftPin);
  void startLeft();
  void startRight();
  void stop();

private:
  uint8_t _rightPin;
  uint8_t _leftPin;
  void _rampUp(uint8_t drivePin);
};
