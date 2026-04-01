#include "PWMChannel.h"

PWMChannel::PWMChannel(Adafruit_PWMServoDriver& drv, uint8_t ch)
  : driver(drv), channel(ch), currentPWM(0), targetPWM(0), minPWM(0), autoOnOffEnabled(false),
    pauseOnMs(0), pauseOffMs(0), pauseStartMs(0),
    startPWM(0), transitionStartMs(0), transitionDurationMs(0),
    phaseOffsetMs(0), phaseStartMs(0), phaseDelayActive(false) {}

void PWMChannel::setPWM(uint16_t pwm) {
  currentPWM = pwm;
  driver.setPWM(channel, 0, pwm);
}

void PWMChannel::startTransitionTo(uint16_t target) {
  startPWM = currentPWM;
  targetPWM = target;
  transitionStartMs = millis();
  uint16_t dist = (target > currentPWM) ? (target - currentPWM) : (currentPWM - target);
  // same total time as linear: one STEP_INTERVAL_MS per SPEED units
  transitionDurationMs = (dist == 0) ? 0 : ((uint32_t)dist * STEP_INTERVAL_MS + SPEED - 1) / SPEED;
}

void PWMChannel::setValue(uint16_t pwm) {
  targetPWMMemory = pwm;
  startTransitionTo(pwm);
}

void PWMChannel::setSpeed(uint16_t s) {
  SPEED = s;
  if (currentPWM != targetPWM) startTransitionTo(targetPWM);
}

void PWMChannel::setOffset(uint16_t offset) {
  phaseOffsetMs = offset;
  phaseDelayActive = (offset > 0);
  phaseStartMs = 0;
}

void PWMChannel::setBrightness(uint8_t brightness) {
  setPWM(gammaCorrectedPWM(brightness));
}

void PWMChannel::on() {
  setPWM(4095);
}

void PWMChannel::off() {
  setPWM(0);
}

uint16_t PWMChannel::gammaCorrectedPWM(uint8_t brightness) {
  const float gamma = 2.2;
  float normalized = brightness / 100.0;
  return (uint16_t)(pow(normalized, gamma) * 4095.0);
}

uint16_t PWMChannel::getCurrentPWM() const {
  return currentPWM;
}

uint8_t PWMChannel::getChannel() const {
  return channel;
}

void PWMChannel::autoOnOff(boolean enable) {
  autoOnOffEnabled = enable;
}

void PWMChannel::setPause(uint32_t on, uint32_t off) {
  pauseOnMs = on;
  pauseOffMs = off;
}

void PWMChannel::setMinValue(uint16_t minVal) {
  minPWM = minVal;
}

void PWMChannel::loop() {
  if (phaseDelayActive) {
    if (phaseStartMs == 0) phaseStartMs = millis();
    if (millis() - phaseStartMs < phaseOffsetMs) return;
    phaseDelayActive = false;
    if (currentPWM != targetPWM) transitionStartMs = millis();
  }

  if (currentPWM != targetPWM) {
    if (transitionDurationMs == 0) {
      setPWM(targetPWM);
    } else {
      uint32_t elapsed = millis() - transitionStartMs;
      if (elapsed >= transitionDurationMs) {
        setPWM(targetPWM);
      } else {
        float t = (float)elapsed / (float)transitionDurationMs;
        float eased = (1.0f - cosf(t * (float)M_PI)) * 0.5f;
        setPWM((uint16_t)(startPWM + (int32_t)(targetPWM - startPWM) * eased));
      }
    }
  } else if (autoOnOffEnabled && targetPWM > minPWM) {
    if (pauseOnMs == 0) {
      startTransitionTo(minPWM);
    } else if (pauseStartMs == 0) {
      pauseStartMs = millis();
    } else if (millis() - pauseStartMs >= pauseOnMs) {
      pauseStartMs = 0;
      startTransitionTo(minPWM);
    }
  } else if (autoOnOffEnabled && targetPWM <= minPWM) {
    if (pauseOffMs == 0) {
      startTransitionTo(targetPWMMemory);
    } else if (pauseStartMs == 0) {
      pauseStartMs = millis();
    } else if (millis() - pauseStartMs >= pauseOffMs) {
      pauseStartMs = 0;
      startTransitionTo(targetPWMMemory);
    }
  }
}
