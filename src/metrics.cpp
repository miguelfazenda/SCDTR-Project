#include "metrics.h"
#include "glob.h"

float Metrics::powerConsumption()
{
    return nominalPower * luminaire.controller.u/255.0;
}

float Metrics::energyConsumption()
{
    return powerConsumption()*luminaire.samplingTime;
}

void Metrics::visibilityErrorUpdate()
{
    float newSample = max(0, luminaire.luxRefAfterConsensus - currentLux);
    visibilityError = (visibilityError*numberOfSamples + newSample)/(numberOfSamples+1);
}

void Metrics::flickerErrorUpdate()
{    
    if(numberOfSamples >= 3)
    {
        int flickerFlag = (currentLux-previousLuxSamles[0]) * (previousLuxSamles[0]-previousLuxSamles[1]);

        float newSample = 0;
        if(flickerFlag < 0)
        {
            newSample = abs(currentLux-previousLuxSamles[0]) + abs(previousLuxSamles[0]-previousLuxSamles[1]);
            newSample = newSample/(2*luminaire.samplingTime); //Confirmar se é este o tempo que queremos
        }
        else
        {
            newSample = 0;
        }
        
        flickerError = (flickerError*(numberOfSamples-2) + newSample)/(numberOfSamples-1);
    }

    previousLuxSamles[1] = previousLuxSamles[0];
    previousLuxSamles[0] = currentLux;
    
}

void Metrics::updateMetrics()
{
    float voltage = luminaire.getVoltage();
    currentLux = luminaire.voltageToLux(voltage);//Confirmar a dimensão

    energyConsumedSinceLastReset += energyConsumption();

    numberOfSamples += 1;
}