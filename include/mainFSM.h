#ifndef FSM_H
#define FSM_H

#include "calibrationFSM.h"

enum State { Init, WakeupWait, Calibrate, Run };

class MainFSM
{
private:
    State state;

    unsigned long lastTransitionTime;
public:
    MainFSM();
    void setState(State state);
    void loop();

    void runStateInit(unsigned long timeSinceLastTransition);
    void runStateWakeupWait(unsigned long timeSinceLastTransition);
};

#endif