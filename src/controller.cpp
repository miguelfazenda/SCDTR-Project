#include "controller.h"
#include "util.h"
#include <Arduino.h> //Só serve para os prints

#define KP 10//15
#define KI 800

Controller::Controller()
{
	ui = 0.0f;
}

int Controller::calc(float errorVoltage, unsigned long samplingTime, int luxRef, float systemGain) {
	int uff; // Feedforward control signal (PWM)
	float up; // Feedback control signal (PWM)
	int u; // Control signal (PWM)

	//Feedforward
	uff = luxRef/systemGain;
	
	//PI control
	ui = ui + samplingTime*(KI/1000000.0f)*errorVoltage; // samplingTime/1000000.0f to convert it to seconds
	up = KP*errorVoltage;

	//Calculate control signal
	u = uff + up + ui;

	//Anti-windup (limits the integrator from getting so large the control signal gets off bounds), and limits the sinal to [0, 255]
	if(u > 255) {
		ui = 255 - uff - up;
		u = 255;
	} else if(u<0) {
		ui = 0 - uff - up;
		u = 0;
	}

	/**
	 * Prints controller variables
	 */
	/*Serial.print("\tuff=");
	Serial.print(uff);
	Serial.print("\tup=");
	Serial.print(KP*errorVoltage);
	Serial.print("\tdeltat=");
	Serial.print(samplingTime);
	Serial.print("\tdeltaui=");
	Serial.print(samplingTime*(KI/1000000.0f)*errorVoltage);
	Serial.print("\tui=");
	Serial.println(ui);*/

	return u;
}