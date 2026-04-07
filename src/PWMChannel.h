#ifndef PWM_CHANNEL_H
#define PWM_CHANNEL_H

#include <Arduino.h>
#include <Adafruit_PWMServoDriver.h>

#define MAX_BRIGHTNESS 4095
#define LESS_BRIGHTNESS 2500

class PWMChannel {
private:
  uint16_t SPEED = 50;
  static const uint16_t STEP_INTERVAL_MS = 10;

  Adafruit_PWMServoDriver& driver;
  uint8_t channel;
  uint16_t currentPWM;
  uint16_t targetPWM;
  uint16_t targetPWMMemory;
  uint16_t minPWM;
  bool autoOnOffEnabled;
  uint32_t pauseOnMs;
  uint32_t pauseOffMs;
  uint32_t pauseStartMs;

  // sine easing transition state
  uint16_t startPWM;
  uint32_t transitionStartMs;
  uint32_t transitionDurationMs;

  // phase offset (initial delay before first cycle)
  uint32_t phaseOffsetMs;
  uint32_t phaseStartMs;
  bool phaseDelayActive;

  void startTransitionTo(uint16_t target);

public:
  PWMChannel(Adafruit_PWMServoDriver& drv, uint8_t ch);

  void setPWM(uint16_t pwm);
  void setValue(uint16_t pwm);
  void setBrightness(uint8_t brightness);
  void on();
  void off();
  void autoOnOff(boolean enable);
  void setSpeed(uint16_t s);
  void setOffset(uint16_t offset);
  void setPause(uint32_t on, uint32_t off);
  void setMinValue(uint16_t minVal);
  uint16_t getCurrentPWM() const;
  uint8_t getChannel() const;
  void loop();
};

#endif
