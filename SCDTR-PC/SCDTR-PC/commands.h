#ifndef COMMANDS_H
#define COMMANDS_H

#include <string>
#include <memory>
#include "command.h"
#include "ServerConnection.h"

class Program;

class Commands
{
public:
    static Command interpretCommand(std::string line, std::ostream* textOutputStream,
        std::shared_ptr<ServerConnection> clientSession);
    static std::shared_ptr<Program> program;
private:
    static uint8_t commandGetDestination(const std::string & substring);
    static uint8_t checkGetArguments(std::string *data);
    static std::pair<uint8_t, float> checkSetArgumentsFloat(const std::string& data);
    static std::pair<uint8_t, bool> checkSetArgumentsBool(const std::string& data);
};

#endif