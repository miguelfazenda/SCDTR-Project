#ifndef _SERIAL_COMM_H
#define _SERIAL_COMM_H

#include <Arduino.h>
#include "command.h"

//TODO meter isto num sitio de jeito
union Convertion
{
    uint32_t value32b;
    uint8_t valueBytes[4];
    float valueFloat;
};

#define SERIAL_FREQUENT_DATA_PACKET_SIZE 6
class SerialFrequentDataPacket {
public:
    SerialFrequentDataPacket(uint8_t node, float iluminance, uint8_t pwm)
    {
        this->node = node;
        this->iluminance = iluminance;
        this->pwm = pwm;
    }

    SerialFrequentDataPacket(uint8_t* buffer)
    {
        this->node = buffer[0];
        this->iluminance = float((uint32_t)(buffer[1]) << 24 |
            (uint32_t)(buffer[2]) << 16 |
            (uint32_t)(buffer[3]) << 8 |
            (uint32_t)(buffer[4]));

        this->pwm = buffer[5];
    }

    size_t toByteArray(uint8_t* array)
    {
        //Convert float to 4 bytes
        Convertion illuminanceConvertion;
        illuminanceConvertion.valueFloat = iluminance;

        array[0] = node;
        array[1] = illuminanceConvertion.valueBytes[0];
        array[2] = illuminanceConvertion.valueBytes[1];
        array[3] = illuminanceConvertion.valueBytes[2];
        array[4] = illuminanceConvertion.valueBytes[3];
        array[5] = pwm;
    }

    void sendOnSerial()
    {
        //Convert float to 4 bytes
        Convertion illuminanceConvertion;
        illuminanceConvertion.valueFloat = iluminance;

        Serial.write(255);
        Serial.write('F');
        Serial.write(node);
        Serial.write(illuminanceConvertion.valueBytes[0]);
        Serial.write(illuminanceConvertion.valueBytes[1]);
        Serial.write(illuminanceConvertion.valueBytes[2]);
        Serial.write(illuminanceConvertion.valueBytes[3]);
        Serial.write(pwm);
        Serial.flush();
    }

    uint8_t node;
    float iluminance;
    int8_t pwm;
};

class SerialComm
{
private:
    // The number of nodes that still haven't sent their result
    uint8_t numNodesWaitingResult;

    //If the last command received was a 'total command', means it is waiting for the response
    bool runningTotalCommand;
    
    // The accumulator that will contain the result of a 'total command' (g I T),
    // i.e. the sum of the result of the command for each node
    float totalCommandResult;

    //If the PC responded to the last discovery message sent
    bool pcDiscoveryHadResponse;
public:
    void readSerial();
    uint32_t executeCommand(Command command);
    void sendResponse(uint32_t value);
    void sendResponse(float value);
    void receivedCommandResponseFromCAN(uint8_t sender, uint32_t value);

    void sendTotalIfAllValueForTotalReceived();
    void sendPCDiscovery();
};

#endif