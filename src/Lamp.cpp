#include "Lamp.h"

void Lamp::off() {
    pWhiteLed->setValue(0);
    for (int i=0; i<9; i++) {
        pChevron[i]->setValue(0); 
    }
    for (int i=0; i<18; i++) {
        pBlueLed[i]->setSpeed(10);
        pBlueLed[i]->setValue(0);
    }
}

void Lamp::onButtonClick() {
    lampMode++;
    if (lampMode == LAMP_RESET) {
        lampMode = LAMP_OFF;
    }

    switch(lampMode) {
        case LAMP_OFF:
            off();
        break; 
        case LAMP_CH:
            off();
            for (int i=0; i<9; i++) {
                pChevron[i]->setValue(MAX_BRIGHTNESS); 
            }
        break;
        case LAMP_WHITE:
            off();
            pWhiteLed->setValue(MAX_BRIGHTNESS);
        break;
        case LAMP_BLUE:
            off();
            for (int i=0; i<18; i++) {
                pBlueLed[i]->setSpeed(10);
                pBlueLed[i]->setValue(MAX_BRIGHTNESS);
            }
        break;
        case LAMP_CH_WHITE: 
            off();
            for (int i=0; i<9; i++) {
                pChevron[i]->setValue(MAX_BRIGHTNESS); 
            }
            pWhiteLed->setValue(MAX_BRIGHTNESS);
        break;
        case LAMP_CH_BLUE:
            off();
            for (int i=0; i<9; i++) {
                pChevron[i]->setValue(MAX_BRIGHTNESS); 
            }
            for (int i=0; i<18; i++) {
                pBlueLed[i]->setSpeed(10);
                pBlueLed[i]->setValue(MAX_BRIGHTNESS);
            }
        break;
        case LAMP_ALL:
            off();
            pWhiteLed->setValue(MAX_BRIGHTNESS);
            for (int i=0; i<9; i++) {
                pChevron[i]->setValue(MAX_BRIGHTNESS); 
            }
            for (int i=0; i<18; i++) {
                pBlueLed[i]->setSpeed(10);
                pBlueLed[i]->setValue(MAX_BRIGHTNESS);
            }
        break;
    }
}