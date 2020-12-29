#include "luminaire.h"
#include <Arduino.h>
#include <EEPROM.h> 

#include "glob.h"

#define CONTROL_DELAY 10000						//100 Hz corresponds to 1/100 s = 10000us
#define LPF_SAMPLING_DELAY (CONTROL_DELAY / 80) // LPF sample 80 times in one control_delay

//#define LDR_SLOPE_B 1.967f  //CHAGAS 48.5 LUX
//#define LDR_SLOPE_B 2.20f ZE		48.5 LUX

//Static function
float Luminaire::getVoltage() {
	return (analogRead(LDR_PIN))*5/1024.0f;
}

void Luminaire::init(bool occupied) {
	pinMode(LED_PIN, OUTPUT);
	pinMode(LDR_PIN, INPUT);

	//Luminair physical variables, saved on the EEPROM
	EEPROM.get(EEPROM_ADDR_SLOPEM, ldrSlopeM);
	EEPROM.get(EEPROM_ADDR_SLOPEB, ldrSlopeB);
	EEPROM.get(EEPROM_ADDR_COST, cost);

	// Serial.print("--------[Physical Param]--------");
	// Serial.println(ldrSlopeM);
	// Serial.println(ldrSlopeB);
	// Serial.println(nodeId);
	// Serial.print("--------[End Physical Param]--------");

	//Calibrates
	//initialCalibration();

	// Changes the desired LUX value
	setOccupied(occupied);

	//So that the lpf has at least one sample to start with
	//lpf.sample();
}

void Luminaire::loop() {
	unsigned long timeNow;
	unsigned long samplingTime; //Time since last sampling IN MICROSECONDS!

	//Calculates time since last sample
	timeNow = micros();
	samplingTime = timeNow - lastSampleTime;

	//Only samples after CONTROL_DELAY elapsed since last sample
	if (samplingTime >= CONTROL_DELAY)
	{
		if (lastSampleTime == 0)
		{
			/* Corrects sampling time: If this is the first sample, lastSampleTime=0,
			*	and samplingTime will be a big meaningless number, since there haven't been any previous samples */
			samplingTime = 0;
		}

		//Samples the voltage
		control(timeNow, samplingTime);

		lastSampleTime = timeNow;
	}

	//The low pass filter measures the voltage in intervals of 1/80th the CONTROL_DELAY
	if (timeNow - lpfSampleTime > LPF_SAMPLING_DELAY)
	{
		lpf.sample();
		lpfSampleTime = timeNow;
	}
}

/**
 * Samples the voltage at the sensor and chages the LED PWM according to the controller
 */
void Luminaire::control(unsigned long timeNow, unsigned long samplingTime)
{
	//Measure error compared to expected voltage
	float expectedVoltage = simulator.simulate(timeNow);
	//float measuredVoltage = getVoltage();
	float measuredVoltage = lpf.value(); //Gets voltage from the low-pass filter
	float errorVoltage = expectedVoltage - measuredVoltage;

	//Ignores very small errors as they are caused by noise
	if (abs(errorVoltage) <= 0.01)
	{
		errorVoltage = 0;
	}

	//Apply error to the controller to get the PWM value
	int pwm = 0;//controller.calc(errorVoltage, samplingTime, luxRef, systemGain);

	//Apply PWM SIGNAL
	analogWrite(LED_PIN, pwm);

	/*
	 * 	 Prints useful data
	 */

#if false //Choose between which prints we want
	//Serial monitor prints
	Serial.print("V=");
	Serial.print(measuredVoltage);
	Serial.print("\tsimV=");
	Serial.println(expectedVoltage);
	Serial.print("PWM=");
	Serial.print(pwm);
	Serial.print("\te=");
	Serial.print(errorVoltage);
	Serial.print("\tLUX=");
	Serial.println(voltageToLux(measuredVoltage));
#else
	//Serial plotter prints
	Serial.print(voltageToLux(measuredVoltage));
	Serial.print(",");
	Serial.println(voltageToLux(expectedVoltage));
#endif
}

/**
 * Performs the initial system calibration:
 * 		- Calculates the gain of the system (LUX/PWM) 
 */
void Luminaire::initialCalibration()
{
	Serial.println("\nStarting calibration");

	const int pwm1 = 20;
	const int pwm2 = 240;

	//Calibrate gain
	analogWrite(LED_PIN, pwm1);
	delay(1000);
	int lux1 = voltageToLux(getVoltage());

	analogWrite(LED_PIN, pwm2);
	delay(1000);
	int lux2 = voltageToLux(getVoltage());
	systemGain = (float)(lux1-lux2) / (pwm1-pwm2);

	Serial.print(F("Calibration successful!\t Gain="));
	Serial.println(systemGain, 8);
	Serial.print(F(" from PWM="));
	Serial.print(pwm1);
	Serial.print(" lux=");
	Serial.print(lux1);
	Serial.print(" and PWM=");
	Serial.print(pwm2);
	Serial.print(" lux=");
	Serial.println(lux2);
}

/**
 * Sets the lux reference, and starts a step on the simulator 
 */
void Luminaire::setLuxRef(int lux)
{
	luxRef = lux;
	simulator.startStep(getVoltage(), luxToVoltage(luxRef));
}

/**
 * Changes occupied state. (Calls setLuxRef)
 */
void Luminaire::setOccupied(bool o) {
	occupied = o;

	int lux = occupied ? LUX_OCCUPIED : LUX_NON_OCCUPIED;
	setLuxRef(lux);

	Serial.print(F("OCCUPIED\tReference:\tLux="));
	Serial.print(lux);
	Serial.print("\tV=");
	Serial.println(luxToVoltage(lux));
}




float Luminaire::luxToVoltage(int lux) {
	float rLDR = pow(10, ldrSlopeM*log10(lux)+ldrSlopeB);
	return 5 * R1_val / (R1_val + rLDR);
}

float Luminaire::voltageToLux(float voltage) {
	float rLDR = R1_val*(5-voltage)/voltage; //(5/vR1-1)*R1_val;
	return (float)pow(10, (log10(rLDR)- ldrSlopeB)/ldrSlopeM);
}