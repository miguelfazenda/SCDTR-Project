#ifndef METRICS_H
#define METRICS_H

#include <Arduino.h>

class Metrics
{
private:
   const float nominalPower = 1; //[W]
   unsigned long numberOfSamples = 0;
   float previousLuxSamples[2] = {0}; //Position [0] keeps the last sample, and position [1] keeps the second to last sample
   float currentLux = 0;

   float energyConsumption();
   void visibilityErrorUpdate();
   void flickerErrorUpdate();
public:
   float energyConsumedSinceLastReset = 0; //[J]
   float flickerError = 0;                 //[LUX/s]
   float visibilityError = 0;              //[LUX]

   void updateMetrics();
   float powerConsumption();

   void resetMetrics();
};

#endif