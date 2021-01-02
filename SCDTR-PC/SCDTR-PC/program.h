#ifndef PROGRAM_H
#define PROGRAM_H

#include <boost/asio.hpp>

#include <vector>
#include <utility>
#include <map>
#include <queue>
#include "command.h"
#include "ServerConnection.h"

#define TCP_PORT 8082
using namespace std;

class Program : public std::enable_shared_from_this<Program>
{
private:
    void commandTimedOut(const boost::system::error_code& e);

    void start_read_console();
    void tcp_start_accept();

    std::vector<std::shared_ptr<ServerConnection>> clientSessions;


    boost::asio::io_context& io_context;
    boost::asio::ip::tcp::acceptor acceptor;

    //Command timeout timer, that every 3 seconds checks if a command had no response
    unique_ptr<boost::asio::steady_timer> timerCommandTimeout;


    //create copy of console stream descriptor
    //boost::asio::posix::stream_descriptor console_stm_desc{io_context, ::dup(STDIN_FILENO)};
    boost::asio::windows::stream_handle console_stm_desc{ io_context, CreateFile(L"CONIN$", GENERIC_READ,
        FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED | FILE_FLAG_NO_BUFFERING, NULL) };
    //the stream to receive the console input
    boost::asio::streambuf console_stm_buff{ 1024 };

    //std::shared_ptr<ServerConnection> newSession;


public:
    Program(boost::asio::io_context& io_context);

    void start(const char* serialStr);

    void run_service();

    void writeToSerialPort(boost::asio::mutable_buffer buf);
    void start_read_serial();
    void handle_read_serial(const boost::system::error_code &err, size_t len);

    void receivedFrequentData(uint8_t nodeId, uint8_t pwm, float iluminance);

    void addToLastMinuteBuffer(uint8_t nodeId, uint8_t pwm, float iluminance);
    void removeOldLastMinuteBufferEntries();
    void printLastMinuteBuffer(const uint8_t nodeId, const bool pwm, std::ostream* textOutputStream);

    void addCommandToQueue(Command command, shared_ptr<ServerConnection> client);
    void executeNextInCommandQueue();
    /*
     * Shows the response and moves to the next command in the queue
     * Called when a response to a command was received on the Serial
     */
    void receivedCommandResponse(uint32_t value);

    //If it's detected the serial hasn't read enough, the unfinished part is stored in this string
    //  so that it's used the next time data arrives
    string unfinishedSerialString;

    vector<int> nodeIds;

    //Map: nodeId, vector
    //Vector: buffer containing pairs of <time, reading>
    //Reading: pair containing pwm(uint8_t) and iluminance(float)
    map<uint8_t, vector<pair<unsigned long, pair<uint8_t, float>>>> lastMinuteBuffer;

    queue<CommandQueueElement> commandsQueue;

    //Streaming active to the server console
    //char -> '\0' not streaming, 'I' streaming variable I, 'd' streaming d
    char streamingActive = 0;
    
    // pair<clientSession, char> - clientSession can be nullptr to indicate streaming to the server console
    //std::vector<std::pair<std::shared_ptr<ServerConnection>, char>> clientsStreamingActive;

    boost::asio::serial_port serial_port;
    boost::asio::streambuf serial_stm_buff{ 1024 };
};

#endif