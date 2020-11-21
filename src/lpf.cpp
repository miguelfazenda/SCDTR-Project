#include "lpf.h"
#include <Arduino.h>
#include "luminaire.h"
/**
 * Sample the voltage and store it on lpfSamples (moving average vector)
 */
void LPF::sample()
{
	float measuredVoltage = Luminaire::getVoltage(LDR_PIN);
	lpfSamples[lpfIdx] = measuredVoltage;

	//Cycles the index to write from [0, 19], and stores the total amount of valid samples on the vector
	lpfIdx++;
	lpfNumSamples = max(lpfIdx, lpfNumSamples); // increases lpfNumSamples on the first cycle, then stays at LPF_TOTAL_SAMPLES
	if (lpfIdx >= LPF_TOTAL_SAMPLES)
	{
		lpfIdx = 0;
	}
}

/**
 * Gets the low-pass filter value by calculating the moving average of the last 20 samples
 */
float LPF::value()
{
	float lpfAvgVal = 0;
	for (int i = 0; i < lpfNumSamples; i++)
	{
		lpfAvgVal += lpfSamples[i];
	}
	lpfAvgVal /= lpfNumSamples;
	return lpfAvgVal;
}
