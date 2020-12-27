#ifndef _SERIAL_COMM_H
#define _SERIAL_COMM_H

#include <Arduino.h>

class SerialComm
{
private:
    void readCommandGet(uint8_t getParam, uint8_t destination);
    void readCommandSet(uint8_t setParam, uint8_t destination, float value);
public:
    SerialComm();
    void sendFrequentData();
    void readSerial();
};

#endif