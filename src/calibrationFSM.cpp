#include "calibrationFSM.h"
#include <Arduino.h>
#include "glob.h"

CalibrationFSM::CalibrationFSM()
{
    state = CalibrationState::CalibInit;
}

void CalibrationFSM::setState(CalibrationState state)
{
    this->state = state;
    lastTransitionTime = micros();
}

void CalibrationFSM::runStateInit(unsigned long timeSinceLastTransition) {
    float residualReading = luminaire.voltageToLux(luminaire.getVoltage());
    //communication.sendCalibReady(residualReading);
}

void CalibrationFSM::loop() {
    unsigned long currentTime = micros();
    unsigned long timeSinceLastTransition = lastTransitionTime;

    switch (state)
    {
    case CalibrationState::CalibInit:
        runStateInit(currentTime);
        break;
    default:
        break;
    }
}

void CalibrationFSM::setNodeReady(uint8_t nodeId, float data) {
    //nodesReady++;
    //if all nodes ready
    //   cenas


}