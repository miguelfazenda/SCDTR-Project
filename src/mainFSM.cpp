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
 *  INIT - Wait 2 senconds, then send broadcast wakeup.
 *      It waits so that it doesn't broadcast imediatly after turning on,
 *        which run the risk of other nodes not being ready wet to listen 
 *      Next State -> WakeupWait
 */
void MainFSM::runStateInit(unsigned long timeSinceLastTransition) {
    if(timeSinceLastTransition > 2000000) {
        communication.sendBroadcastWakeup();
        Serial.println("[MainFSM] State = WakeupWait");
        setState(State::WakeupWait);
    }
}

/**
 * WakeupWait - Waits 2 seconds, then prints the list of nodes and goes to the calibration
 *      This state is for it to have time to receive all the other node's Ids.
 *      Next State -> Calibrate
 */
void MainFSM::runStateWakeupWait(unsigned long timeSinceLastTransition) {
    if(timeSinceLastTransition > 2000000) {
        Serial.println("[MainFSM] Finished waiting wakeup");
        
        //TODO talvez as linhas e colunas estejam trocadas
        //Prints list of nodes
        Serial.print("Nodes list: [");
        for(uint8_t i = 0; i<numTotalNodes; i++) {
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
        if(calibrationFSM.done) {
            setState(State::Run);



            //Print gain matrix
            calibrationFSM.printResidualAndGainMatrix();
        }
        break;
    case State::Run:
        //Serial.println(luminaire.getVoltage());
        break;
    default:
        break;
    }
}