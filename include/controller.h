#ifndef CONTROLLER_H
#define CONTROLLER_H

#include "simulador.h"

class Controller
{
private:
    float ui; // Feedback integrator control signal
public:
    float u; // Control signal (PWM)

    Controller();

    /**
     * Returns the control signal (PWM signal to be applied to LED)
     * @param gain The gain of the system
     */
    int calc(float errorVoltage, unsigned long samplingTime, float luxRef, float systemGain, float residualRead);
    void controllerReset();
};

#endif