#include "commands.h"

#include <iostream>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>

#include "input_exception.h"
#include "Server.h"
#include "command.h"

std::shared_ptr<Server> Commands::program;

uint8_t Commands::commandGetDestination(const std::string & substring)
{
    int destination;
    try
    {
        destination = std::stoi(substring);
    }
    catch (std::exception&)
    {
        throw input_exception("Destination must be an integer");
    }

    if (destination > 255 || destination <= 0)
    {
        throw input_exception("Invalid destination");
    }

    //searches the nodeIds vector to see if this destination is present there
    program->mtxNodeIds.lock();
    if (std::find(program->nodeIds.begin(), program->nodeIds.end(), destination) == program->nodeIds.end())
    {
        program->mtxNodeIds.unlock();
        throw input_exception("Destination ID " + to_string(destination) + " not present");
    }
    program->mtxNodeIds.unlock();

    return destination;
}

/**
 * Parses the string and returns the destination
 *      Throws exception input_exception if the string is in wrong format
 */
uint8_t Commands::checkGetArguments(std::string *data)
{
    std::string arguments = "IdoOULxRrcptevf"; //string that has every char thats corresponds to
                                              //one argument of comands type get
    std::string argumentsWithT = "pevf";      //string that has every char thats corresponds to
                                              //one argument of comands type get that allows the last argument to be 'T'

    if ((*data).length() < 5 || (*data)[3] != ' ')
    {
        throw input_exception("Invalid command");
    }

    //Checks if arguments contains the char at position 2 of the data string
    if (arguments.find((*data)[2]) == std::string::npos)
    {
        throw input_exception("No valid argument");
    }

    //Check if is a Total get
    if ((*data)[4] == 'T')
    {
        //Has a T at the end
        //Check if the argument is a valid one for T requests
        if (argumentsWithT.find((*data)[2]) != std::string::npos)
        {
            //Return 0 means it's a total
            return 0;
        }
        else
        {
            throw input_exception("Argument" + std::to_string((*data)[2]) + " is not valid for total");
        }
    }

    //Gets the destination from the command
    auto lastPartOfString = (*data).substr(3, (*data).length() - 1);
    return commandGetDestination(lastPartOfString);
}

/**
 * Parses the string, returns the destination and the value to be set
 *      Throws exception input_exception if the string is in wrong format
 * @param data string
 */
pair<uint8_t, float> Commands::checkSetArgumentsFloat(const std::string& data)
{
    uint8_t destination = 0;
    size_t idx_second_space = 0;
    float value;

    if (data.length() < 5 || data[1] != ' ')
    {
        throw input_exception("Invalid command");
    }

    idx_second_space = data.find(' ', 2);
    if (idx_second_space == std::string::npos)
    {
        throw input_exception("Invalid command");
    }

    destination = commandGetDestination(data.substr(2, idx_second_space - 1));

    //Contains the value
    auto lastPartOfString = data.substr(idx_second_space + 1, data.length() - 1);

    //Value is expected to be a float
    try
    {
        value = stof(lastPartOfString);
    }
    catch (std::exception&)
    {
        throw input_exception("Value must be a number");
    }

    return make_pair(destination, value);
}

/**
 * Parses the string, returns the destination and the value to be set
 *      Throws exception input_exception if the string is in wrong format
 * @param data string
 */
pair<uint8_t, bool> Commands::checkSetArgumentsBool(const std::string& data)
{
    uint8_t destination = 0;
    size_t idx_second_space = 0;
    float value;

    if (data.length() < 5 || data[1] != ' ')
    {
        throw input_exception("Invalid command");
    }

    idx_second_space = data.find(' ', 2);
    if (idx_second_space == std::string::npos)
    {
        throw input_exception("Invalid command");
    }

    destination = commandGetDestination(data.substr(2, idx_second_space - 1));

    //Contains the value
    auto lastPartOfString = data.substr(idx_second_space + 1, data.length() - 1);

    //Value is expected to be a float
    try
    {
        value = stof(lastPartOfString);
    }
    catch (std::exception&)
    {
        throw input_exception("Value must be a number");
    }

    if(value != 0 && value != 1)
        throw input_exception("Value must be 0 or 1");

    return make_pair(destination, value);
}

//TODO isto pode levar um parametro que é o stream onde vai escrever (ou cout para terminal, ou socket)
Command Commands::interpretCommand(std::string line, std::ostream* textOutputStream,
    std::shared_ptr<ServerConnection> clientSession)
{
    if (line.length() >= 1 && line[0] == 'q')
    {
        return Command((char)line[0], 0, '\0');
    }
    else if (line.length() >= 1 && line[0] == 'r')
    {
        return Command((char)line[0], 0, '\0');
    }
    if (line.length() > 2 && line[1] == ' ')
    {
        uint8_t destination;

        try
        {
            if (line[0] == 'g')
            {
                destination = checkGetArguments(&line);
                /*std::cout << line[2] << " "
                            << "Destination: " << destination << std::endl;*/
                return Command((char)line[0], destination, (char)line[2]);
            }
            else if (line[0] == 'O' || line[0] == 'U' || line[0] == 'c')
            {
                //command type set
                auto destinationAndValue = checkSetArgumentsFloat(line);
                uint8_t destination = destinationAndValue.first;
                float value = destinationAndValue.second;

                return Command((char)line[0], destination, value);
            }
            else if(line[0] == 'o')
            {
                //command type set
                auto destinationAndValue = checkSetArgumentsBool(line);
                uint8_t destination = destinationAndValue.first;
                bool boolValue = destinationAndValue.second;

                /**textOutputStream << line[0] << " "
                        << "Destination: " << destination
                        << "Value: " << (boolValue == 0 ? "false" : "true") << std::endl;*/

                return Command((char)line[0], destination, (char)boolValue);
            }
            else if(line[0] == 'b')
            {
                destination = checkGetArguments(&line);
                
                //command = Command((char)line[0], destination, (char)line[2]);
                if(line[2] == 'I')
                    program->lastMinuteBuffer2.printLastMinuteBuffer(destination, false, textOutputStream);
                else if(line[2] == 'd')
                    program->lastMinuteBuffer2.printLastMinuteBuffer(destination, true, textOutputStream);
                else
                    *textOutputStream << "Must be buffer 'I' or 'd': " << line << std::endl;
            }
            else if (line[0] == 's')
            {
                destination = checkGetArguments(&line);

                //Locks mutex for the streaming active
                std::lock_guard<std::mutex> lockMtxStreamingActive(program->mtxStreamingActive);
                
                //Weather it's the server that sent the command or a client
                bool server = (clientSession.get() == nullptr);

                //StreamingActive variable pointer of either the server or a client
                char* streamingActive = server ? &program->streamingActive : &clientSession->streamingActive;

                if(*streamingActive == 0)
                {
                    //This client(or the server) is not streaming. Starts streaming
                    char typeOfStream = line[2];
                    if (typeOfStream == 'I' || typeOfStream == 'd')
                    {
                        *streamingActive = line[2];
                    }
                    else
                    {
                        *textOutputStream << "Must be buffer 'I' or 'd': " << line << std::endl;
                    }
                }
                else
                {
                    //Stops streaming
                    *streamingActive = 0;
                }
            }
            else
            {
                //no messagem type recognized
                *textOutputStream << "Unknown command 🤔: " << line << std::endl;
            }
        }
        catch (std::exception &e)
        {
            *textOutputStream << e.what() << std::endl;
        }
    }
    else
    {
        *textOutputStream << "Unknown command 🤔: " << line << std::endl;
    }

    return Command();
}