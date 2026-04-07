#pragma once
#include <stdio.h>
#include "AudioPlayer.h"
#include "Motor.h"
#include "PWMChannel.h"

class DialingUp {
public:
    DialingUp(AudioPlayer* pAudio, Motor* pMotor, PWMChannel** pChevron, PWMChannel** pBlueLed, PWMChannel* pWhiteLed);

    void dial();
    void incomming();
    void loop();

    boolean isBussy();

private:
    bool direction = false;
    uint16_t stage = 0;
    uint8_t round = 0; //dialing
    uint8_t openTimeout; //ile razy czekamy w pętli 103 - 104
    uint8_t chevronSequence[7];
    AudioPlayer* pAudio;
    Motor* pMotor;
    PWMChannel** pChevron;
    PWMChannel** pBlueLed;
    PWMChannel* pWhiteLed;
    void setWaitFor(uint32_t mils, uint16_t nextStage);
    uint16_t  _nextStage;
    uint32_t _waiting;
    uint32_t waitInMs;
    uint32_t getRandomRotationTime();
    uint16_t cancelStage;
    void blueLedSetup(int i);
    

    //stages functions:
    void dialingLoop();
    void openingGate();
    void incommingCall();
    void dialFail();

    //special stage:
    void waitingLoop();
};
