#ifndef _COMMAND_H
#define _COMMAND_H

#include <iostream>
#include "ServerConnection.h"

class Command {
public:
    Command();
    Command(char cmd, uint8_t destination, char type);
    Command(char cmd, uint8_t destination, float value);

    Command(uint8_t* buffer);
    size_t toByteArray(uint8_t* array);

    void setType(char t);
    char getType();
    void setValue(float v);
    float getValue();

    int8_t cmd;  // Get (g), set(o, U, ..)
    uint8_t destination; //Destino
    uint32_t value; // Type (g I, g d) ou set value (float)
};

struct CommandQueueElement
{
    Command command;
    // (Can be nullprt to represent that it was send by the server itself)
    std::shared_ptr<ServerConnection> client;
};

#endif