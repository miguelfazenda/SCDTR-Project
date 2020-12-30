#ifndef _COMMAND_H
#define _COMMAND_H

#include <Arduino.h>

class Command {
public:
    Command();
    Command(char cmd, uint8_t destination, char type);
    Command(char cmd, uint8_t destination, float value);

    Command(uint8_t* buffer);
    size_t toByteArray(char* array);

    void setType(char t);
    char getType();
    void setValue(float v);
    float getValue();

    int8_t cmd;  // Get (g), set(o, U, ..)
    uint8_t destination; //Destino
    uint32_t value; // Type (g I, g d) ou set value (float)
};

#endif