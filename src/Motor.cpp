#include "Motor.h"

Motor::Motor(uint8_t rightPin, uint8_t leftPin)
  : _rightPin(rightPin), _leftPin(leftPin) {
  pinMode(_rightPin, OUTPUT);
  pinMode(_leftPin, OUTPUT);
  analogWrite(_rightPin, 0);
  analogWrite(_leftPin, 0);
}

void Motor::startLeft() {
  analogWrite(_rightPin, 0);
  analogWrite(_leftPin, 255);
}

void Motor::startRight() {
  analogWrite(_leftPin, 0);
  analogWrite(_rightPin, 255);
}

void Motor::stop() {
  analogWrite(_rightPin, 0);
  analogWrite(_leftPin, 0);
}
