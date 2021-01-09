#include "metrics.h"
#include "glob.h"

float Metrics::powerConsumption()
{
    return nominalPower * luminaire.controller.u / 255.0;
}

float Metrics::energyConsumption()
{
    return powerConsumption() * luminaire.samplingTime/1000000;
}

void Metrics::visibilityErrorUpdate()
{
    float newSample = max(0, luminaire.luxRefAfterConsensus - currentLux);
    visibilityError = (visibilityError * numberOfSamples + newSample) / (numberOfSamples + 1);
}

void Metrics::flickerErrorUpdate()
{
    if (numberOfSamples >= 3)
    {
        int flickerFlag = (currentLux - previousLuxSamples[0]) * (previousLuxSamples[0] - previousLuxSamples[1]);

        float newSample = 0;
        if (flickerFlag < 0)
        {
            newSample = abs(currentLux - previousLuxSamples[0]) + abs(previousLuxSamples[0] - previousLuxSamples[1]);
            newSample = newSample / (2 * luminaire.samplingTime/1000000.0f);
        }
        else
        {
            newSample = 0;
        }

        flickerError = (flickerError * (numberOfSamples - 2) + newSample) / (numberOfSamples - 1);
    }

    previousLuxSamples[1] = previousLuxSamples[0];
    previousLuxSamples[0] = currentLux;
}

void Metrics::updateMetrics()
{
    float voltage = luminaire.getVoltage();
    currentLux = luminaire.voltageToLux(voltage); //Confirmar a dimensão

    energyConsumedSinceLastReset += energyConsumption();
    flickerErrorUpdate();
    visibilityErrorUpdate();

    numberOfSamples += 1;
}

void Metrics::resetMetrics()
{
    numberOfSamples = 0;
    previousLuxSamples[0] = 0;
    previousLuxSamples[1] = 0;
    currentLux = 0;
    energyConsumedSinceLastReset = 0;
    flickerError = 0;
    visibilityError = 0;
}