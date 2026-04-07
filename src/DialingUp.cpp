#include "DialingUp.h"
#include "snd_roll.h"
#include "snd_roll_start.h"
#include "snd_ch_lock.h"
#include "snd_gate_open.h"
#include "snd_gate_close.h"
#include "snd_wormhole_loop.h"
#include "snd_ch_in1.h"
#include "snd_ch_in2.h"
#include "snd_ch_in3.h"
#include "snd_ch_in4.h"
#include "snd_ch_in5.h"
#include "snd_ch_in6.h"
#include "snd_ch_in7.h"
#include "snd_dial_fail.h"

#define MIN_BLUE_VALUE 10
#define MID_BLUE_VALUE 200
#define MAX_BLUE_VALUE MAX_BRIGHTNESS

DialingUp::DialingUp(AudioPlayer* pAudio, Motor* pMotor, PWMChannel** pChevron, PWMChannel** pBlueLed, PWMChannel* pWhiteLed) {
    this->pAudio = pAudio;
    this->pMotor = pMotor;
    this->pChevron = pChevron;
    this->pBlueLed = pBlueLed;
    this->pWhiteLed = pWhiteLed;
    chevronSequence[0] = 1;
    chevronSequence[1] = 2;
    chevronSequence[2] = 3;
    chevronSequence[3] = 6;
    chevronSequence[4] = 7;
    chevronSequence[5] = 8;
    chevronSequence[6] = 0;
}

void DialingUp::loop() {
    dialingLoop();
    openingGate();
    incommingCall();
    waitingLoop();
    dialFail();
}

void DialingUp::dialingLoop() {
    switch(stage) {
        case 1:
            cancelStage = 300;
            pAudio->play(snd_roll_start, sizeof(snd_roll_start));
            pAudio->loopNext(snd_roll, sizeof(snd_roll));
            if (direction) {
                pMotor->startLeft();
            } else {
                pMotor->startRight();
            }
            setWaitFor(getRandomRotationTime(), 2);
            break;
        case 2:
            pMotor->stop();
            pAudio->stop();
            pAudio->play(snd_ch_lock, sizeof(snd_ch_lock));
            pChevron[0]->setValue(MAX_BRIGHTNESS);
            setWaitFor(1200, 3);
            break;
        case 3:
            pChevron[0]->setValue(0);
            pChevron[chevronSequence[round]]->setValue(LESS_BRIGHTNESS);   
            direction = !direction;         
            setWaitFor(2400, 4);
            break;
        case 4:
            round++;
            if (round == 7) {
                stage = 100; //goto opening gate
            } else {
                stage=1; // next round
            }
            break;
    }
}

void DialingUp::openingGate() {
    switch(stage) {
        //opening
        case 100:
            cancelStage = 105;
            for(int i=0; i<9; i++) {
                pChevron[i]->setValue(800);
            }
            pAudio->play(snd_gate_open, sizeof(snd_gate_open));
            pAudio->loopNext(snd_wormhole_loop, sizeof(snd_wormhole_loop));
            setWaitFor(500, 101);
            break;
        case 101:
            pWhiteLed->autoOnOff(true);
            pWhiteLed->setValue(MAX_BRIGHTNESS);
            pWhiteLed->setSpeed(50);

            for(int i=0; i<18; i++) {
                pBlueLed[i]->autoOnOff(true);
                blueLedSetup(i);
            }
            setWaitFor(1000, 102);
            break; 
        case 102: 
            pWhiteLed->setValue(600);
            pWhiteLed->autoOnOff(false);
            openTimeout = random(15, 20);
            stage++;
            break;
        case 103:
            if (openTimeout ==0) {
                stage = 105;
            } else {
                setWaitFor(3000, 104);
            }
            openTimeout--;
            break;
        case 104:
            for(int i=0; i<18; i++) {
                if (random(0,3) > 1) {
                    continue;
                }
                blueLedSetup(i);
            }
            stage = 103;
            break;
        case 105:
            pAudio->play(snd_gate_close, sizeof(snd_gate_close));
            pWhiteLed->setValue(MAX_BRIGHTNESS);
            for (int i=0; i<9; i++) {
                pChevron[i]->setValue(MAX_BRIGHTNESS); 
            }
            for (int i=0; i<18; i++) {
                pBlueLed[i]->setSpeed(10);
                pBlueLed[i]->setValue(0);
            }
            setWaitFor(2400, 106);
            break;
        case 106: 
            pWhiteLed->setValue(0);
            for (int i=0; i<9; i++) {
                pChevron[i]->setValue(0); 
            }
            stage = 0; //the end;
            round = 0;
            break;
    }
}

void DialingUp::incommingCall() {
   switch(stage) {
        case 200:
            cancelStage = 300;
            switch(round) {
                case 0:
                    pAudio->play(snd_ch_in1, sizeof(snd_ch_in1));
                break;
                case 1:
                    pAudio->play(snd_ch_in2, sizeof(snd_ch_in2));
                break;
                case 2:
                    pAudio->play(snd_ch_in3, sizeof(snd_ch_in3));
                break;
                case 3:
                    pAudio->play(snd_ch_in4, sizeof(snd_ch_in4));
                break;
                case 4:
                    pAudio->play(snd_ch_in5, sizeof(snd_ch_in5));
                break;
                case 5:
                    pAudio->play(snd_ch_in6, sizeof(snd_ch_in6));
                break;
                case 6:
                    pAudio->play(snd_ch_in7, sizeof(snd_ch_in7));
                break;
            }
            
            pChevron[chevronSequence[round]]->setValue(LESS_BRIGHTNESS);   
            if (round == 6) {
                setWaitFor(1000, 201);
            } else {
                setWaitFor(1900, 201);
            }
            break;
        case 201:
            round++;
            if (round == 7) {
                stage = 100; //opening
            } else {
                stage = 200;
            }
        break;
   }
}

void DialingUp::dialFail() {
    switch(stage) {
        case 300:
            pAudio->play(snd_dial_fail, sizeof(snd_dial_fail));
            pMotor->stop();
            setWaitFor(2100, 301);
        break;
        case 301: 
            for (int i=0; i<18; i++) {
                pBlueLed[i]->setSpeed(10);
                pBlueLed[i]->setValue(0);
            }
            pWhiteLed->setValue(0);
            for (int i=0; i<9; i++) {
                pChevron[i]->setValue(0); 
            }
            stage = 0;
            round = 0;
        break;
    }
}

void DialingUp::waitingLoop() {
    if (stage == 1000) {
        if ( millis() - _waiting > waitInMs) {
                stage = _nextStage;
        }
    }
}

void DialingUp::dial() {
    if (stage > 0) {
        stage = cancelStage;
        return;
    } 

    for(int i=0; i<9; i++) {
        pChevron[i]->off();
    }
    for (int i=0; i<18; i++) {
        pBlueLed[i]->autoOnOff(false);
        pBlueLed[i]->off();
    }
    pWhiteLed->off();
    round = 0;
    stage = 1;
}

void DialingUp::incomming() {
    if (stage > 0) {
        return;
    } 

    for(int i=0; i<9; i++) {
        pChevron[i]->off();
    }
    for (int i=0; i<18; i++) {
        pBlueLed[i]->autoOnOff(false);
        pBlueLed[i]->off();
    }
    pWhiteLed->off();
    round = 0;
    stage = 200;
}

void DialingUp::setWaitFor(uint32_t mils, uint16_t nextStage) {
    waitInMs = mils;
    _waiting = millis();
    _nextStage = nextStage;
    stage = 1000;
}



uint32_t DialingUp::getRandomRotationTime() {
    return 2000 + (600 * random(4, 11));
}

void DialingUp::blueLedSetup(int i) {
    pBlueLed[i]->setValue(random(MID_BLUE_VALUE, MAX_BLUE_VALUE));
    pBlueLed[i]->setMinValue(random(MIN_BLUE_VALUE, MID_BLUE_VALUE));
    pBlueLed[i]->setSpeed(random(30, 100));
}

boolean DialingUp::isBussy() {
    return stage > 0;
}