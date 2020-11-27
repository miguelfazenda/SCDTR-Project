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
        Serial.println("[MainFSM] State = WakeupWait");
        setState(State::WakeupWait);
    }
}

void MainFSM::runStateWakeupWait(unsigned long timeSinceLastTransition) {
    if(timeSinceLastTransition > 2000000) {
        Serial.println("[MainFSM] Finished waiting wakeup");
        
        //Prints list of nodes
        Serial.print("Nodes list: [");
        for(int i = 0; i<numTotalNodes; i++) {
            if(i > 0)
                Serial.print(", ");
            Serial.print(nodesList[i]);
        }
        Serial.println("]");
        
        Serial.println("[MainFSM] State = Calibrate");
        setState(State::Calibrate);
    }
}

void MainFSM::loop() {
    unsigned long currentTime = micros();
    unsigned long timeSinceLastTransition = currentTime - lastTransitionTime;

    switch (state)
    {
    case State::Init:
        runStateInit(timeSinceLastTransition);
        break;
    case State::WakeupWait:
        runStateWakeupWait(timeSinceLastTransition);
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