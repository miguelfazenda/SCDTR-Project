#ifndef CALIBRATION_FSM_H
#define CALIBRATION_FSM_H

#include <Arduino.h>

enum CalibrationState { CalibInit, WaitReady, WaitLedOn,WaitTransient };

class CalibrationFSM
{
private:
    CalibrationState state;

    

    int nodeLedOn;

    int luxReadNum; //flag that checks the number of the read  ( we need 2 read to compute the gain)
    unsigned int pwm[2];
    
    float luxReads[2];
    unsigned long lastTransitionTime;
public:
    unsigned int numNodesReady;
    bool otherNodeLedOn;
    float gainMatrix[3][3]={0};
    CalibrationFSM();
    void setState(CalibrationState state);
    void loop();
    
    void setNodeReady(uint8_t nodeId, float data);

    void runStateInit();
    void runStateWaitReady();
    void runStateWaitLedOn();
    void runStateWaitTrasient(unsigned long timeSinceLastTransition);
};

#endif