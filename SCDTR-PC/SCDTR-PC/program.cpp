#include "program.h"
#include <boost/bind.hpp>
#include <iostream>
#include <ctime>
#include <thread>

#include "convertion.h"
#include "commands.h"
using boost::asio::ip::tcp;
using namespace std;


Program::Program(boost::asio::io_context& io)
    : io_context { io },
    acceptor(io_context, tcp::endpoint(tcp::v4(), TCP_PORT)),
    serial_port { io_context }
{
    //Creates the command timeout timer
    timerCommandTimeout = make_unique<boost::asio::steady_timer>(
        boost::asio::steady_timer{ io_context, boost::asio::chrono::seconds {3} }
    );
}

void Program::run_service()
{
    io_context.run();
}

void Program::start(const char* serialStr)
{
    // Setup serial
    boost::system::error_code ec;
    // connect to port
    serial_port.open(serialStr, ec);
    if (ec)
    {
        throw std::runtime_error("Could not open serial port\n");
    }
    serial_port.set_option(boost::asio::serial_port_base::baud_rate{ 1000000 }, ec);

    start_read_console();
    start_read_serial();
    tcp_start_accept();
}


void Program::start_read_console()
{
    async_read_until(console_stm_desc, console_stm_buff, '\n',
        [this](const boost::system::error_code& err, std::size_t len)
        {
            //When user inputed something on the console. console_stm_buff contains the value

            std::istream input(&console_stm_buff);
            std::string line;
            getline(input, line, '@');

            Command command = Commands::interpretCommand(line, &cout, shared_ptr<ServerConnection>(nullptr));
            if (command.cmd != '\0')
            {
                //Means the a command was read from the line (invalid command will have cmd=='\0')    
                //The client will be set to a null pointer to indicate it was sent by the server itself
                addCommandToQueue(command, shared_ptr<ServerConnection>(nullptr));
            }

            start_read_console();
        }
    );
}


void Program::tcp_start_accept()
{
    //The connection to a client
    //Has to be a pointer because it is passed to the handle_accept and here it goes out of scope
    const auto newSession = make_shared<ServerConnection>(io_context, weak_from_this());

    acceptor.async_accept(newSession->socket,
        [this, newSession](const boost::system::error_code& err)
        {
            //Connection accepted!
            //TODO thread unsafe!
            clientSessions.push_back(newSession);

            newSession->start();
            //Accept the next client
            tcp_start_accept();
        });
}

void Program::writeToSerialPort(boost::asio::mutable_buffer buf)
{
    async_write(serial_port, buf, [this](const boost::system::error_code& err, std::size_t len)
    {});
}

/**
 * Called when received something from the serial port
 */
void Program::handle_read_serial(const boost::system::error_code &err, size_t len)
{
    istream input(&serial_stm_buff);
    string line;
    getline(input, line, '@');

    //If the last message was unfinished, merge it with the received part
    if (!unfinishedSerialString.empty())
    {
        //Remaining part of string from last time received serial
        line.insert(0, unfinishedSerialString);
        unfinishedSerialString.clear();
    }
    

    //Find sync character (255)
    size_t offset = 0;
    size_t syncBytePos = string::npos;

    while((syncBytePos = line.find((char)255, offset)) != string::npos)
    {
        //Size of the message (default 2 - 1 byte for 255 another for the char 'F', 'D', etc.)
        int sizeMsg = 2;

        //If it's detected the serial hasn't read enough, the unfinished part must be stored
        //  so that it's used the next time data arrives
        // This happens because the read_until is reading until 255, but sometimes 255 appears inside messages
        bool unfinished = false;

        if (line.length() - syncBytePos >= 2)
        {
            if (line[syncBytePos + 1] == 'F')
            {
                //Reads frequent data that was just received
                sizeMsg = 6; //Tamanho da mensagem em bytes (a contar com o sync byte e com o 'F')
                if (line.length() - syncBytePos >= sizeMsg)
                {
                    uint8_t nodeId = (uint8_t)line[syncBytePos + 2];

                    Convertion convertion;
                    convertion.valueBytes[0] = line[syncBytePos + 3];
                    convertion.valueBytes[1] = line[syncBytePos + 4];
                    
                    float iluminance = (float)convertion.value16b / 10;

                    uint8_t pwm = (uint8_t)line[syncBytePos + 5];

                    receivedFrequentData(nodeId, pwm, iluminance);
                }
                else
                {
                    //std::cout << "Warning: too short" << std::endl;
                    unfinished = true;
                }
            }
            else if (line[syncBytePos + 1] == 'R')
            {
                //Response to command

                sizeMsg = 5; //Tamanho da mensagem em bytes (a contar com o sync byte e com o 'F')
                if (line.length() - syncBytePos >= sizeMsg)
                {
                    uint32_t value = (uint32_t)((uint8_t)line[syncBytePos + 2]) << 24 |
                        (uint32_t)((uint8_t)line[syncBytePos + 3]) << 16 |
                        (uint32_t)((uint8_t)line[syncBytePos + 4]) << 8 |
                        (uint32_t)((uint8_t)line[syncBytePos + 5]);

                    receivedCommandResponse(value);
                }
                else
                {
                    //std::cout << "Warning: too short" << std::endl;
                    unfinished = true;
                }
            }
            else if (line[syncBytePos + 1] == 'D')
            {
                //Arduino sent "Discovery message"
                //This means the arduino wants to know if it's connected to a PC.
                //Respond back with a command cmd='D' (doesn't go in the command Queue because we dont need to wait for a response)
                char buf[7];
                buf[0] = 255;
                Command('D', 0, ' ').toByteArray(buf + 1);
                writeToSerialPort(boost::asio::buffer(buf, 7));
            }
            else if (line[syncBytePos + 1] == 'L')
            {
                //Arduino sent nodes list
                //This contains the a list of the IDs of each node
                //Has at least another 1 byte (for number of nodes)
                sizeMsg = 3;
                if (line.length() - syncBytePos >= sizeMsg)
                {
                    //Reads the number of nodes. Therefore the message occupies more numNodes bytes
                    int numNodes = (uint8_t)line[syncBytePos + 2];
                    sizeMsg = 3 + numNodes;

                    if (line.length() - syncBytePos >= sizeMsg)
                    {
                        //Reads the bytes in the message where each contain the ID
                        nodeIds.clear();
                        cout << "Received list of nodes";
                        for (int i = 0; i < numNodes; i++)
                        {
                            int nodeId = (uint8_t)line[syncBytePos + 3 + i];
                            nodeIds.push_back(nodeId);
                            cout << nodeId << ", ";
                        }
                        cout << endl;
                    }
                    else
                    {
                        //std::cout << "Warning: too short" << std::endl;
                        unfinished = true;
                    }
                }
                else
                {
                    //std::cout << "Warning: too short" << std::endl;
                    unfinished = true;
                }
            }

            if (unfinished)
            {
                unfinishedSerialString = line.substr(syncBytePos);
            }
        }

        //Advances offset so it searches for next sync byte
        offset = syncBytePos + sizeMsg;
    }

    cout << "[SERIAL] " << line << endl;

    start_read_serial();
}

void Program::start_read_serial()
{
    //Ele está a ler até encontrar o byte 255.
    // isso faz com que só chame o handle quando a proxima mensagem enviar o seu byte inicial 255
    async_read_until(serial_port, serial_stm_buff, 255, boost::bind(&Program::handle_read_serial, this,
        boost::asio::placeholders::error,
        boost::asio::placeholders::bytes_transferred));
}

void Program::receivedCommandResponse(uint32_t value)
{
    if (commandsQueue.size() > 0)
    {
        /*
         * Prints the command
         */

        //Cancels the timeout timer as this command didn't time out
        timerCommandTimeout->cancel();

        auto commandQueueElement = commandsQueue.front();

        //Writes the command response here if it's to be redirected to a client
        std::ostringstream textOutputForClient;

        //To which stream the text should go (cout or textOutputForClient)
        std::ostream* textOutputStream = nullptr;
        if (commandQueueElement.client.get() == nullptr)
            // The text from this command should be written on the cout
            textOutputStream = &cout;
        else
            textOutputStream = &textOutputForClient;


        Command& command = commandQueueElement.command;
        if (command.cmd == 'g')
        {
            //Destination can be a number or a char 'T'
            string destination = (command.destination == 0 ? "T" : to_string(command.destination));

            //Weather to print the result as a 1/0 or as a float
            char type = command.getType();
            if (type == 'o')
            {
                //Occupancy can be 1 or 0, prints value
                *textOutputStream << command.cmd << " "
                    << destination << " " << value << endl;
            }
            else
            {
                //Other get commands return a float
                Convertion convert;
                convert.value32b = value;

                *textOutputStream << command.getType() << " "
                    << destination << " " << convert.valueFloat << endl;
            }
        }
        else if (command.cmd == 'o' || command.cmd == 'O' || command.cmd == 'U' || command.cmd == 'c' || command.cmd == 's')
        {
            //Print ack or err 
            *textOutputStream << (value == 1 ? "ack" : "err") << endl;
        }

        //Sends the text Output of this command to the client
        if (commandQueueElement.client.get() != nullptr)
            commandQueueElement.client->sendMessage(textOutputForClient.str());


        //Removes this command from the queue
        commandsQueue.pop();
        //Executes next command in queue
        if (!commandsQueue.empty())
            executeNextInCommandQueue();
    }
    else
    {
        cout << "Warning: Received response " << (float)value << " for command, but command queue is empty";
    }
}

void streamValue(char streamingActive, std::ostream& textOutputStream, uint8_t pwm, float iluminance)
{
    if (streamingActive == 'd')
        textOutputStream << time(0) << " " << pwm * 100 / 255 << "%" << endl;
    else if (streamingActive == 'I')
        textOutputStream << time(0) << " " << iluminance << endl;
}

/*
 * Received a frequent data packet from a the HUB (packet originated from any node)
 *   add it to the last minute buffer
 *   if the streaming is active stream it
*/
void Program::receivedFrequentData(uint8_t nodeId, uint8_t pwm, float iluminance)
{
    //If the streaming mode is active in the server, print it to cout
    if (streamingActive != 0)
        streamValue(streamingActive, cout, pwm, iluminance);

    for (auto clientSession : clientSessions)
    {
        if (clientSession->streamingActive != 0)
        {
            std::ostringstream textOutputForClient;
            //If the streaming mode is active in the client, print it to the buffer textOutputForClient
            streamValue(clientSession->streamingActive, textOutputForClient, pwm, iluminance);
            //Sends the text in the buffer
            clientSession->sendMessage(textOutputForClient.str());
        }
    }

    addToLastMinuteBuffer(nodeId, pwm, iluminance);
}


void Program::addToLastMinuteBuffer(uint8_t nodeId, uint8_t pwm, float iluminance)
{
    removeOldLastMinuteBufferEntries();
    time_t timeNow = time(0);
    
    //Note: buffer[nodeId] automatically inserts a new entry if key "nodeId" doesnt exist yet
    lastMinuteBuffer[nodeId].push_back(make_pair((unsigned long)timeNow, make_pair(pwm, iluminance)));
}

/**
 * Removes all the items from the last minute buffet where the time is older than 1 minute ago
 */
void Program::removeOldLastMinuteBufferEntries()
{
    unsigned long timeThreshold = (unsigned long)time(0) - 60;

    for (auto nodeBufferKeyValue : lastMinuteBuffer)
    {
        //For each pair nodeId,Buffer
        auto nodeBuffer = nodeBufferKeyValue.second;

        //Find where an entry's time is older than 1 minute ago and erase
        auto item = begin(nodeBuffer);
        while (item != end(nodeBuffer))
        {
            if (item->first < timeThreshold)
            {
                item = nodeBuffer.erase(item);
            }
            else
            {
                //The first element to be found is the oldest, therefore no other element needs to be deleted
                break;
            }
        }
    }
}

/**
 * Prints a last minute buffer's contents
 * @param pwm : true: print PWM, false: print Iluminance
 * @param textOutputStream: where to print to. Can be &std::cout or a buffer to send to a client for ex.
 */
void Program::printLastMinuteBuffer(const uint8_t nodeId, const bool pwm, std::ostream* textOutputStream)
{
    //Prints header
    cout << "b " << (pwm ? 'd' : 'I') << ' ' << (int)nodeId << endl;

    if (lastMinuteBuffer.find(nodeId) == lastMinuteBuffer.end())
    {
        //Has no data for such nodeId
        return;
    }
   
    removeOldLastMinuteBufferEntries();


    auto nodeBuffer = lastMinuteBuffer[nodeId];

    //Get last element pointer
    auto lastElement = nodeBuffer.size() > 0 ? &nodeBuffer.back() : nullptr;

    for (auto entry = nodeBuffer.begin(); entry != nodeBuffer.end(); ++entry)
    {  
        if (pwm)
        {
            *textOutputStream << entry->second.first * 100 / 255 << "%";
        }
        else
        {
            *textOutputStream << entry->second.second;
        }

        
        //If it's the last element dont print the comma ','
        if(&(*entry) == lastElement)
            *textOutputStream << endl;
        else
            *textOutputStream << "," << endl;
    }
}

void Program::addCommandToQueue(Command command, shared_ptr<ServerConnection> client)
{
    commandsQueue.push(CommandQueueElement{ command, client });

    //Execute this command if it's the only one in the queue,
    // If not, it means another command is awaiting the response
    if(commandsQueue.size() == 1)
        executeNextInCommandQueue();
}

void Program::executeNextInCommandQueue()
{
    if (commandsQueue.empty())
        return;

    //Converts the command to a byteArray and sends it with a sync byte first (255)
    char buf[7];
    buf[0] = 255;
    commandsQueue.front().command.toByteArray(buf + 1);
    
    writeToSerialPort(boost::asio::buffer(buf, 7));

    //Starts the timeout timer - it is canceled if there is a reponse in time.
    //  If not the commandTimedOut function is ran 
    timerCommandTimeout->expires_after(boost::asio::chrono::seconds{ 3 });
    timerCommandTimeout->async_wait(
        boost::bind(&Program::commandTimedOut, this,
            boost::asio::placeholders::error)
    );
}

/*
 * The last command went too long without a response
 */
void Program::commandTimedOut(const boost::system::error_code& e)
{
    if (e != boost::asio::error::operation_aborted)
    {
        std::cout << "Command timed out!" << std::endl;

        //Executes the next command

        //Removes this command from the queue
        commandsQueue.pop();
        //Executes next command in queue
        if (!commandsQueue.empty())
            executeNextInCommandQueue();
    }
}