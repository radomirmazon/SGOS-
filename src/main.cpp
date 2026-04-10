#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include "PWMChannel.h"
#include "Motor.h"
#include "AudioPlayer.h"
#include "DialingUp.h"
#include "Button.h"
#include "Lamp.h"
#include "config.h"

// --- MAX98357A I2S pins ---
// BCLK → GPIO15 | LRC → GPIO16 | DIN → GPIO17 | SD → GPIO18 | GAIN → GND (9 dB)
AudioPlayer audio(15, 16, 17, 18);

// PCA9685 modules - addresses 0x40 (0) and 0x41 (1)
Adafruit_PWMServoDriver pca0 = Adafruit_PWMServoDriver(0x40);
Adafruit_PWMServoDriver pca1 = Adafruit_PWMServoDriver(0x41);


Motor motor = Motor(5, 6);

// --- Test silnika: co 60 s kręci przez 10 s, naprzemiennie lewo/prawo ---
static unsigned long motorLastRun  = 0;
static unsigned long motorStarted  = 0;
static bool          motorRunning  = false;
static bool          motorDirLeft  = true;
static constexpr uint32_t MOTOR_INTERVAL_MS = 60000UL;
static constexpr uint32_t MOTOR_RUN_MS      = 10000UL;

// PWM parameters
#define PWM_FREQ 1000  // 1kHz for LEDs (no visible flicker)
#define MAX_PWM 4095

PWMChannel* pChevron[CHEVRON_TABLE_SIZE];
PWMChannel* pBlueLed[BLUE_TABLE_SIZE];
PWMChannel* pWhiteLed;

DialingUp* pDialup;

Button dialButton;

Lamp* pLamp = nullptr;

// --- Losowe wywołania incoming co 3–15 minut ---
static unsigned long nextIncomingMs = 0;

static unsigned long randomIncomingInterval() {
  return (unsigned long)random(15UL * 60000UL, 45UL * 60000UL);
}

void setup() {


  audio.begin();  // Uruchamia I2S i zadanie na Core 0

  // Initialize both PCA9685 modules
  pca0.begin();
  pca1.begin();

  // Set PWM frequency (default 50Hz for servos)
  pca0.setPWMFreq(PWM_FREQ);
  pca1.setPWMFreq(PWM_FREQ);

  delay(10);

  // Turn off all channels
  for (uint8_t ch = 0; ch < 16; ch++) {
    pca0.setPWM(ch, 0, 0);
    pca1.setPWM(ch, 0, 0);
  }

  pChevron[0] = new PWMChannel(pca0, 0);
  pChevron[1] = new PWMChannel(pca0, 1);
  pChevron[2] = new PWMChannel(pca0, 2);
  pChevron[3] = new PWMChannel(pca0, 3);
  pChevron[4] = new PWMChannel(pca0, 4);
  pChevron[5] = new PWMChannel(pca0, 5);
  pChevron[6] = new PWMChannel(pca0, 6);
  pChevron[7] = new PWMChannel(pca0, 7);
  pChevron[8] = new PWMChannel(pca0, 8);
  
  pBlueLed[0] = new PWMChannel(pca0, 9);
  pBlueLed[1] = new PWMChannel(pca0, 10);
  pBlueLed[2] = new PWMChannel(pca1, 0);
  pBlueLed[3] = new PWMChannel(pca0, 11); 
  pBlueLed[4] = new PWMChannel(pca0, 12);
  pBlueLed[5] = new PWMChannel(pca0, 13);
  pBlueLed[6] = new PWMChannel(pca0, 14);
  pBlueLed[7] = new PWMChannel(pca0, 15);
  pBlueLed[8] = new PWMChannel(pca1, 2);
  pBlueLed[9] = new PWMChannel(pca1, 10);
  pBlueLed[10] = new PWMChannel(pca1, 4);
  pBlueLed[11] = new PWMChannel(pca1, 3);
  pBlueLed[12] = new PWMChannel(pca1, 9);
  pBlueLed[13] = new PWMChannel(pca1, 1); 
  pBlueLed[14] = new PWMChannel(pca1, 7);
  pBlueLed[15] = new PWMChannel(pca1, 8); 
  pBlueLed[16] = new PWMChannel(pca1, 5);
  pBlueLed[17] = new PWMChannel(pca1, 6);
  //extra
  pBlueLed[18] = new PWMChannel(pca1, 12);
  pBlueLed[19] = new PWMChannel(pca1, 13);
  pBlueLed[20] = new PWMChannel(pca1, 14);
  pBlueLed[21] = new PWMChannel(pca1, 15);

  pWhiteLed = new PWMChannel(pca1, 11);

  pDialup = new DialingUp(&audio, &motor, pChevron, pBlueLed, pWhiteLed);
  pLamp = new Lamp(pChevron, pBlueLed, pWhiteLed);

  nextIncomingMs = millis() + randomIncomingInterval();

  dialButton.begin(46);  // GPIO46, active low (przycisk do GND)
  dialButton.onShortClick([]() {
    if (!pLamp->isBussy()) {
      pDialup->dial();
    } else {
      pLamp->onButtonClick();
    }
  });

  dialButton.onLongClick([]() {
    if (!pLamp->isBussy()) {
      pDialup->incomming();
    } else {
      pLamp->reset();
    }
  });

  dialButton.onDoubleClick([]() {
    if (!pDialup->isBussy()) {
      pLamp->onButtonClick();
    }
  });

} 

void loop() {

  for (uint8_t i=0; i<CHEVRON_TABLE_SIZE; i++) {
    pChevron[i]->loop();
  }
  for (uint8_t i=0; i<BLUE_TABLE_SIZE; i++) {
    pBlueLed[i]->loop();
  }
  pWhiteLed->loop();
  pDialup->loop();
  dialButton.loop();
  pLamp->loop();

  if (millis() >= nextIncomingMs) {
    nextIncomingMs = millis() + randomIncomingInterval();
    if (!pDialup->isBussy() && !pLamp->isBussy()) {
      pDialup->incomming();
    }
  }
}
