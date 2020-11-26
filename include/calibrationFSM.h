#ifndef CALIBRATION_FSM_H
#define CALIBRATION_FSM_H

#include <Arduino.h>

enum CalibrationState { CalibInit };

class CalibrationFSM
{
private:
    CalibrationState state;

    unsigned long lastTransitionTime;
public:
    CalibrationFSM();
    void setState(CalibrationState state);
    void loop();

    void setNodeReady(uint8_t nodeId, float data);

    void runStateInit(unsigned long timeSinceLastTransition);
};

#endif