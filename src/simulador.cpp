#include "simulador.h"
#include <Arduino.h>

/**
 * Aproximate the time constante for the capacitor to charge to a given function
 */
int getTau(float voltage) {
  return -5194*log(voltage) + 13928;
}

void Simulador::startStep(float vInitial, float vFinal) {
    this->stepTimeInitial = micros();
    this->vInitial = vInitial;
    this->vFinal = vFinal;
    
    this->tau = getTau(vFinal);
}

float Simulador::simulate(unsigned long t) {
    return vFinal - (vFinal - vInitial) * exp(-(float)(t-stepTimeInitial)/tau);
}


