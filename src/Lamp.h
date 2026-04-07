#pragma once
#include <Arduino.h>
#include "PWMChannel.h"

#define LAMP_OFF 0
#define LAMP_CH  1
#define LAMP_WHITE 2
#define LAMP_BLUE 3
#define LAMP_CH_WHITE 4
#define LAMP_CH_BLUE 5
#define LAMP_ALL 6
#define LAMP_RESET 7 

class Lamp {
public:

    Lamp(PWMChannel** pChevron, PWMChannel** pBlueLed, PWMChannel* pWhiteLed) {
        this->pChevron = pChevron;
        this->pBlueLed = pBlueLed;
        this->pWhiteLed = pWhiteLed;
    }

    boolean isBussy() {
        return lampMode != LAMP_OFF;
    }

    void reset() {
        off();
        lampMode = LAMP_OFF;
    }
    void onButtonClick();
    void loop() {}

private:
    void off();
    uint8_t lampMode = LAMP_OFF;
    PWMChannel** pChevron;
    PWMChannel** pBlueLed;
    PWMChannel* pWhiteLed;
};
