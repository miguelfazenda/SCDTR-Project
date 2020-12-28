#ifndef LUMINAIRE_H
#define LUMINAIRE_H

#include "simulador.h"
#include "controller.h"

#define R1_val 10 //valor da resistencia 1 em kilo Ohm

#define LDR_PIN A0 // Analog input pin that the potentiometer is attached to
#define LED_PIN 9 // Analog output pin that the LED is attached to

class Luminaire {
private:
    Simulador simulator;
    Controller controller;
    unsigned long lastSampleTime = 0;
    unsigned long lpfSampleTime = 0;
    const int LUX_OCCUPIED = 20;
    const int LUX_NON_OCCUPIED = 10;
    float ldrSlopeB = 0.0f;
    float ldrSlopeM = 0.0f;

    // LUX/PWM
    float systemGain;

    void control(unsigned long timeNow, unsigned long samplingTime);
    void initialCalibration();
public:
    //TODO ver se o professor prefere que crie um getter
    bool occupied = false;
    int luxRef;

    void init(bool occupied);
    void loop();

    void setLuxRef(int lux);
    void setOccupied(bool o);

    /**
     * FEEDFORWARD CONTROLLER
     * Returns the necessary PWM value to acheive a certain LUX
     */
    int pwmForLux(float lux, float gain);
    float luxToVoltage(int lux);
    float voltageToLux(float voltage);

    float getVoltage();
};

#endif