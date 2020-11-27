#include "mainFSM.h"
#include <Arduino.h>
#include "glob.h"

MainFSM::MainFSM()
{
    state = State::Init;
}

void MainFSM::setState(State state)
{
    this->state = state;
    lastTransitionTime = micros();
}

/**
 *  INIT - Wait 2 senconds, then send broadcast wakeup and 
 */
void MainFSM::runStateInit(unsigned long timeSinceLastTransition) {
    if(timeSinceLastTransition > 2000000) {
        communication.sendBroadcastWakeup();
        setState(State::WakeupWait);
    }
}

void MainFSM::runStateWakeupWait(unsigned long timeSinceLastTransition) {
    if(timeSinceLastTransition > 2000000) {
        setState(State::Calibrate);
    }
}

void MainFSM::loop() {
    unsigned long currentTime = micros();
    unsigned long timeSinceLastTransition = currentTime - lastTransitionTime;

    switch (state)
    {
    case State::Init:
        runStateInit(currentTime);
        break;
    case State::WakeupWait:
        runStateWakeupWait(currentTime);
        break;
    case State::Calibrate:
        calibrationFSM.loop();
        break;
    case State::Run:
        break;
    default:
        break;
    }
}