#ifndef SIMULADOR_H
#define SIMULADOR_H

class Simulador {
  unsigned long stepTimeInitial;
  float vInitial, vFinal; //Voltage
  int tau; //Time constant for vFinal

public:

  /**
   * Called when the lux reference is changed
   */
  void startStep(float vInitial, float vFinal);

  /**
   * Gets the expected voltage at a certain time 
   * @param t time
   */
  float simulate(unsigned long t);
};


#endif