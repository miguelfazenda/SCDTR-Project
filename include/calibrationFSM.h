#ifndef CALIBRATION_FSM_H
#define CALIBRATION_FSM_H

#include <Arduino.h>

#define MAX_NUM_NODES 7 //TODO tentar meter isto no glob.h (não funciona pq é um include ciclico glob.h <--> calibrationFSM.h)

enum CalibrationState { CalibInit, WaitReady, WaitLedOn, WaitTransient };

class CalibrationFSM
{
private:
    CalibrationState state;

    //Indicates which node Index has the LED on. Goes from 0 to numTotalNodes-1. The node id which has the LED on is
    int nodeLedOn;

    //counter that checks the number of the reading (we need 2 readings to compute the gain)
    int luxReadNum;
    unsigned int pwm[2];
    
    float luxReads[2];
    unsigned long lastTransitionTime;
public:
    unsigned int numNodesReady;
    bool otherNodeLedOn;

    //Stores the Ki,j gain, which is the cross-gain beetween node i and node j
    float gainMatrix[MAX_NUM_NODES][MAX_NUM_NODES] = {{0}};
    //Stores the residual reading
    float residualArray[MAX_NUM_NODES] = {0};

    bool done;

    CalibrationFSM();
    ~CalibrationFSM();
    void setState(CalibrationState state);
    void loop();
    
    void incrementNodesReady();
    void receivedGainOrResidual(uint8_t sender, float value);

    void runStateInit();
    void runStateWaitReady();
    void runStateWaitLedOn();
    void runStateWaitTrasient(unsigned long timeSinceLastTransition);

    void setGain(uint8_t i, uint8_t j, float gain);

    void printResidualAndGainMatrix();

    void resetCalib();
};

#endif