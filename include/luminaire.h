#ifndef LUMINAIRE_H
#define LUMINAIRE_H

#include "simulador.h"
#include "controller.h"
#include "calibrationFSM.h"

#define R1_val 10 //valor da resistencia 1 em kilo Ohm

#define LDR_PIN A0 // Analog input pin that the potentiometer is attached to
#define LED_PIN 9 // Analog output pin that the LED is attached to

class Luminaire {
private:
    Simulador simulator;
    Controller controller;
    unsigned long lastSampleTime = 0;
    unsigned long lpfSampleTime = 0;
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
    int luxRefAfterConsensus;
    float cost;

    void init(bool occupied);
    void loop();

    void setLuxRef(int lux);
    void setOccupied(bool o);

    float luxToVoltage(int lux);
    float voltageToLux(float voltage);

    float getVoltage();
};

#endif