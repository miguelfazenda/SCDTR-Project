#include "Server.h"
#include <boost/bind.hpp>
#include <iostream>
#include <ctime>
#include <thread>
#include <memory>

#include "convertion.h"
#include "commands.h"

using boost::asio::ip::tcp;
using boost::asio::ip::udp;
using namespace std;


Server::Server(boost::asio::io_context& io)
    : io_context { io },
    acceptor(io_context, tcp::endpoint(tcp::v4(), TCP_PORT)),
    serial_port { io_context }
    //udpSocket { io_context, udp::endpoint(udp::v4(), TCP_PORT+1) }
{
    //Creates the command timeout timer
    timerCommandTimeout = unique_ptr<boost::asio::steady_timer>(
        new boost::asio::steady_timer{ io_context, boost::asio::chrono::seconds {3} }
    );
}

void Server::start(const char* serialStr)
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
	/*
    udpSocket.async_receive_from(
        boost::asio::buffer(updReceiveBuf), udpRemoteEndpoint,
        [this](const boost::system::error_code& err, std::size_t len)
        {
            udpSocket.async_send_to(boost::asio::buffer(updReceiveBuf), udpRemoteEndpoint,
                [this](const boost::system::error_code& err, std::size_t len)
            {
            });
        }
    );*/
}


void Server::start_read_console()
{
    async_read_until(console_stm_desc, console_stm_buff, '\n',
        [this](const boost::system::error_code& err, std::size_t len)
        {
            //When user inputed something on the console. console_stm_buff contains the value

            std::istream input(&console_stm_buff);
            std::string line;
            getline(input, line, '@');

            Command command = Commands::interpretCommand(line, &cout, shared_ptr<ServerConnection>(nullptr));
            if (command.cmd == 'q')
            {
                quit();
            }
            else if (command.cmd != '\0')
            {
                //Means the a command was read from the line (invalid command will have cmd=='\0')    
                //The client will be set to a null pointer to indicate it was sent by the server itself
                addCommandToQueue(command, shared_ptr<ServerConnection>(nullptr));
            }

            start_read_console();
        }
    );
}

void Server::quit()
{
    io_context.stop();
}

void Server::tcp_start_accept()
{
    //The connection to a client
    //Has to be a pointer because it is passed to the handle_accept and here it goes out of scope
    const auto newSession = make_shared<ServerConnection>(io_context, shared_from_this());

    acceptor.async_accept(newSession->socket,
        [this, newSession](const boost::system::error_code& err)
        {
            //Connection accepted!
            mtxClientSessions.lock();
            clientSessions.push_back(newSession);
            mtxClientSessions.unlock();

            newSession->start();
            //Accept the next client
            tcp_start_accept();
        });
}

void Server::writeToSerialPort(boost::asio::mutable_buffer buf)
{
    async_write(serial_port, buf, [this](const boost::system::error_code& err, std::size_t len)
    {});
}

/**
 * Called when received something from the serial port
 */
void Server::handle_read_serial(const boost::system::error_code &err, size_t len)
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
        size_t sizeMsg = 2;

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
                uint8_t buf[7];
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
                        mtxNodeIds.lock();
                        nodeIds.clear();
                        cout << "Received list of nodes";
                        for (int i = 0; i < numNodes; i++)
                        {
                            int nodeId = (uint8_t)line[syncBytePos + 3 + i];
                            nodeIds.push_back(nodeId);
                            cout << nodeId << ", ";
                        }
                        cout << endl;
                        mtxNodeIds.unlock();
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

void Server::start_read_serial()
{
    //Ele está a ler até encontrar o byte 255.
    // isso faz com que só chame o handle quando a proxima mensagem enviar o seu byte inicial 255
    async_read_until(serial_port, serial_stm_buff, 255, boost::bind(&Server::handle_read_serial, this,
        boost::asio::placeholders::error,
        boost::asio::placeholders::bytes_transferred));
}

void Server::receivedCommandResponse(uint32_t value)
{
    mtxCommandsQueue.lock();
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
        mtxCommandsQueue.unlock();
        executeNextInCommandQueue();
    }
    else
    {
        mtxCommandsQueue.unlock();
        cout << "Warning: Received response " << (float)value << " for command, but command queue is empty";
    }
}

void Server::removeClientSession(shared_ptr<ServerConnection> client)
{
    //Remove client from clientSessions list
    mtxClientSessions.lock();

    auto iterClientToRemove = find(clientSessions.begin(), clientSessions.end(), client);
    clientSessions.erase(iterClientToRemove);
    
    mtxClientSessions.unlock();

    //Removes the pointers to this client in the activeStreams
    mtxStreamingActive.lock();
    for (auto &activeStreamsForNode : activeStreams)
    {
        activeStreamsForNode.second.erase(client);
    }
    mtxStreamingActive.unlock();
}

void streamPWM(std::ostream& textOutputStream, uint8_t pwm)
{
    textOutputStream << time(0) << " " << pwm * 100 / 255 << "%" << endl;
}

void streamIluminance(std::ostream& textOutputStream, float iluminance)
{
    textOutputStream << time(0) << " " << iluminance << endl;
}

/*
 * Received a frequent data packet from a the HUB (packet originated from any node)
 *   add it to the last minute buffer
 *   if the streaming is active stream it
*/
void Server::receivedFrequentData(uint8_t nodeId, uint8_t pwm, float iluminance)
{
    //Locks mutex for the streaming active
    std::lock_guard<std::mutex> lockMtxStreamingActive(mtxStreamingActive);

    //If the streaming mode is active in the server, print it to cout
    
    auto activeStreamsAboutThisNode = activeStreams[nodeId];

    mtxClientSessions.lock();
    for (auto clientActiveStreams : activeStreamsAboutThisNode)
    {
        auto clientSession = clientActiveStreams.first;
        //clientSession nullptr means it's a stream to the server console
        bool isServer = clientSession.get() == nullptr;

        bool streamingPWM = clientActiveStreams.second.first;
        bool streamingIluminance = clientActiveStreams.second.second;

        if (isServer)
        {
            //Stream the value to std::cout
            if (streamingPWM)
                streamPWM(cout, pwm);
            if (streamingIluminance)
                streamIluminance(cout, iluminance);
        }
        else if (streamingPWM || streamingIluminance)
        {
            std::ostringstream textOutputForClient;

            //The streaming mode is active in the client, print it to the buffer textOutputForClient
            if (streamingPWM)
                streamPWM(textOutputForClient, pwm);
            if (streamingIluminance)
                streamIluminance(textOutputForClient, iluminance);

            //Sends the text in the buffer
            clientSession->sendMessage(textOutputForClient.str());
        }
    }
    mtxClientSessions.unlock();

    lastMinuteBuffer2.addToLastMinuteBuffer(nodeId, pwm, iluminance);
}

void Server::addCommandToQueue(Command command, shared_ptr<ServerConnection> client)
{
    mtxCommandsQueue.lock();
    commandsQueue.push(CommandQueueElement{ command, client });
    bool onlyCommandInQueue = (commandsQueue.size() == 1);
    mtxCommandsQueue.unlock();

    //Execute this command if it's the only one in the queue,
    // If not, it means another command is awaiting the response
    if(onlyCommandInQueue)
        executeNextInCommandQueue();
}

void Server::executeNextInCommandQueue()
{
    mtxCommandsQueue.lock();
    if (commandsQueue.empty())
    {
        mtxCommandsQueue.unlock();
        return;
    }

    auto commandQueueElement = commandsQueue.front();

    //Checks if the client has disconnected
    mtxClientSessions.lock();
    auto iterClient = find(clientSessions.begin(),
        clientSessions.end(),
        commandQueueElement.client);
    bool clientNoLongerConnected = (iterClient == clientSessions.end());
    mtxClientSessions.unlock();

    bool isServer = commandQueueElement.client.get() == nullptr;
    if (clientNoLongerConnected && !isServer)
    {
        //Client has disconnected
        //Removes this command from the queue
        commandsQueue.pop();
        mtxCommandsQueue.unlock();

        //Executes next command in queue
        executeNextInCommandQueue();
        return;
    }
    

    //Converts the command to a byteArray and sends it with a sync byte first (255)
    uint8_t buf[7];
    buf[0] = 255;
    commandQueueElement.command.toByteArray(buf + 1);
    mtxCommandsQueue.unlock();
    
    writeToSerialPort(boost::asio::buffer(buf, 7));

    //Starts the timeout timer - it is canceled if there is a reponse in time.
    //  If not the commandTimedOut function is ran 
    timerCommandTimeout->expires_after(boost::asio::chrono::seconds{ 3 });
    timerCommandTimeout->async_wait(
        boost::bind(&Server::commandTimedOut, this,
            boost::asio::placeholders::error)
    );
}

/*
 * The last command went too long without a response
 */
void Server::commandTimedOut(const boost::system::error_code& e)
{
    if (e != boost::asio::error::operation_aborted)
    {
        std::cout << "Command timed out!" << std::endl;

        //Executes the next command

        mtxCommandsQueue.lock();
        //Removes this command from the queue
        commandsQueue.pop();
        mtxCommandsQueue.unlock();

        //Executes next command in queue
        executeNextInCommandQueue();
    }
}