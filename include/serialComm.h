#ifndef _SERIAL_COMM_H
#define _SERIAL_COMM_H

#include <Arduino.h>

class Command {
public:
    Command()
    {
        cmd = '\0';
        destination = 0;
        value = 0;
    }
    Command(char cmd, uint8_t destination, char type)
    {
        this->cmd = cmd;
        this->destination = destination;
        //this->data.type = type;
        setType(type);
    }
    Command(char cmd, uint8_t destination, float value)
    {
        this->cmd = cmd;
        this->destination = destination;
        this->value = value;
    }

    Command(uint8_t* buffer)
    {
        this->destination = buffer[0];
        this->cmd = buffer[1];
        this->value = int((uint32_t)(buffer[2]) << 24 |
            (uint32_t)(buffer[3]) << 16 |
            (uint32_t)(buffer[4]) << 8 |
            (uint32_t)(buffer[5]));
    }

    size_t toByteArray(char* array)
    {
        array[0] = destination;
        array[1] = cmd;
        array[2] = (value >> 24) & 0xFF;
        array[3] = (value >> 16) & 0xFF;
        array[4] = (value >> 8) & 0xFF;
        array[5] = value & 0xFF;
        return 6;
    }

    void setType(char t)
    {
        value = t;
    }
    char getType()
    {
        return (int8_t)value;
    }
    void setValue(float v)
    {
        value = (uint32_t)v;
    }
    float getValue()
    {
        return (float)value;
    }

    int8_t cmd;  // Get (g), set(o, U, ..)
    uint8_t destination; //Destino
    uint32_t value; // Type (g I, g d) ou set value (float)
    
    //Nao deu para usar union devido a endianess e packing e cenas assim :(
    /*union {
        uint32_t value;
        int8_t type;
    } data;*/
};


/*class CommandResponse {
public:
    CommandResponse()
    {
        cmd = '\0';
    }

    CommandResponse(char cmd, uint8_t node, float value)
    {
        this->cmd = cmd;
        this->node = node;
        this->value = value;
    }

    CommandResponse(char* buffer)
    {
        this->cmd = buffer[0];
        this->node = buffer[1];
        this->value = int((uint32_t)(buffer[2]) << 24 |
            (uint32_t)(buffer[3]) << 16 |
            (uint32_t)(buffer[4]) << 8 |
            (uint32_t)(buffer[5]));
    }

    void toByteArray(char* array)
    {
        array[0] = cmd;
        array[1] = node;
        array[2] = (value >> 24) & 0xFF;
        array[3] = (value >> 16) & 0xFF;
        array[4] = (value >> 8) & 0xFF;
        array[5] = value & 0xFF;
    }

    int8_t cmd;
    uint8_t node;
    uint32_t value;
};*/

#define SERIAL_FREQUENT_DATA_PACKET_SIZE 6
class SerialFrequentDataPacket {
public:
    SerialFrequentDataPacket(uint8_t node, float iluminance, uint8_t pwm)
    {
        this->node = node;
        this->iluminance = iluminance;
        this->pwm = pwm;
    }

    SerialFrequentDataPacket(char* buffer)
    {
        this->node = buffer[0];
        this->iluminance = float((uint32_t)((uint8_t)buffer[1]) << 24 |
            (uint32_t)((uint8_t)buffer[2]) << 16 |
            (uint32_t)((uint8_t)buffer[3]) << 8 |
            (uint32_t)((uint8_t)buffer[4]));

        this->pwm = buffer[5];
    }

    /*void toByteArray(char* array)
    {
        uint32_t iluminance4Bytes = (uint32_t)iluminance;

        array[0] = node;
        array[1] = (iluminance4Bytes >> 24) & 0xFF;
        array[2] = (iluminance4Bytes >> 16) & 0xFF;
        array[3] = (iluminance4Bytes >> 8) & 0xFF;
        array[4] = iluminance4Bytes & 0xFF;
        array[5] = pwm;
    }*/

    void sendOnSerial()
    {
        uint32_t iluminance4Bytes = (uint32_t)iluminance;
        Serial.write(255);
        Serial.write('F');
        Serial.write(node);
        Serial.write((iluminance4Bytes >> 24) & 0xFF);
        Serial.write((iluminance4Bytes >> 16) & 0xFF);
        Serial.write((iluminance4Bytes >> 8) & 0xFF);
        Serial.write(iluminance4Bytes & 0xFF);
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
    void readCommandGet(uint8_t getParam, uint8_t destination);
    void readCommandSet(uint8_t setParam, uint8_t destination, float value);

    // The number of nodes that still haven't sent their result
    uint8_t numNodesWaitingResult;

    //If the last command received was a 'total command', means it is waiting for the response
    bool runningTotalCommand;
    
    // The accumulator that will contain the result of a 'total command' (g I T),
    // i.e. the sum of the result of the command for each node
    float totalCommandResult;
public:
    SerialComm();
    void sendFrequentData();
    void readSerial();
    uint32_t executeCommand(Command command);
    void sendResponse(uint32_t value);
    void sendResponse(float value);
    void receivedCommandResponseFromCAN(uint8_t sender, uint32_t value);

    void sendTotalIfAllValueForTotalReceived();
};

#endif