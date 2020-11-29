#ifndef CALIBRATION_FSM_H
#define CALIBRATION_FSM_H

#include <Arduino.h>

enum CalibrationState { CalibInit, WaitReady, WaitLedOn,WaitTransient };

class CalibrationFSM
{
private:
    CalibrationState state;

    int nodeLedOn;

    //counter that checks the number of the reading (we need 2 readings to compute the gain)
    int luxReadNum;
    unsigned int pwm[2];
    
    float luxReads[2];
    unsigned long lastTransitionTime;

    void freeGainMatrix();
    void allocateGainMatrix();
public:
    unsigned int numNodesReady;
    bool otherNodeLedOn;
    
    uint8_t gainMatrixSize;
    float** gainMatrix;

    bool done;

    CalibrationFSM();
    ~CalibrationFSM();
    void setState(CalibrationState state);
    void loop();
    
    void incrementNodesReady();
    void receivedGain(uint8_t sender, float gain);

    void runStateInit();
    void runStateWaitReady();
    void runStateWaitLedOn();
    void runStateWaitTrasient(unsigned long timeSinceLastTransition);

    void setGain(uint8_t i, uint8_t j, float gain);
};

#endif