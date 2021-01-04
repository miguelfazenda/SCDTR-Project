#ifndef PROGRAM_H
#define PROGRAM_H

#include <boost/asio.hpp>

#include <vector>
#include <utility>
#include <map>
#include <queue>
#include <mutex>
#include "command.h"
#include "ServerConnection.h"
#include "LastMinuteBuffer.h"

#include <boost/array.hpp>

#define TCP_PORT 8082
using namespace std;

class Server : public std::enable_shared_from_this<Server>
{
public:
    Server(boost::asio::io_context& io_context);

    void start(const char* serialStr);

    void writeToSerialPort(boost::asio::mutable_buffer buf);
    void start_read_serial();
    void handle_read_serial(const boost::system::error_code &err, size_t len);

    void receivedFrequentData(uint8_t nodeId, uint8_t pwm, float iluminance);

    void addCommandToQueue(Command command, shared_ptr<ServerConnection> client);
    void executeNextInCommandQueue();
    /*
     * Shows the response and moves to the next command in the queue
     * Called when a response to a command was received on the Serial
     */
    void receivedCommandResponse(uint32_t value);

    void removeClientSession(shared_ptr<ServerConnection> client);

    std::mutex mtxNodeIds;
    vector<int> nodeIds;

    //Map: nodeId, vector
    //Vector: buffer containing pairs of <time, reading>
    //Reading: pair containing pwm(uint8_t) and iluminance(float)
    //map<uint8_t, vector<pair<unsigned long, pair<uint8_t, float>>>> lastMinuteBuffer;
    std::mutex mtxCommandsQueue;
    queue<CommandQueueElement> commandsQueue;

    LastMinuteBuffer lastMinuteBuffer2;


    std::mutex mtxStreamingActive;
    //Map - Key: nodeId, Value: a list of active streams (pairs of char and pointer to client session)
    //  The char can be 'I' or 'd' for iluminance and duty-cycle, the pointer can be null to indicate streaming to the server console
    std::map<int, std::map<shared_ptr<ServerConnection>, std::pair<bool, bool>>> activeStreams;
private:
	void commandTimedOut(const boost::system::error_code& e);

	void quit();
	void start_read_console();
	void tcp_start_accept();

	std::mutex mtxClientSessions;
	std::vector<std::shared_ptr<ServerConnection>> clientSessions;


	boost::asio::io_context& io_context;
	boost::asio::ip::tcp::acceptor acceptor;

	/*boost::asio::ip::udp::socket udpSocket;
	boost::array<char, 1> updReceiveBuf;
	boost::asio::ip::udp::endpoint udpRemoteEndpoint;*/

	//Command timeout timer, that every 3 seconds checks if a command had no response
	unique_ptr<boost::asio::steady_timer> timerCommandTimeout;


	//create copy of console stream descriptor
#ifdef _WIN32
	boost::asio::windows::stream_handle console_stm_desc{ io_context, CreateFile(L"CONIN$",
			GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED | FILE_FLAG_NO_BUFFERING, NULL) };
#else
	boost::asio::posix::stream_descriptor console_stm_desc{ io_context, ::dup(STDIN_FILENO) };
#endif
	///the stream to receive the console input
	boost::asio::streambuf console_stm_buff{ 1024 };

	boost::asio::serial_port serial_port;
	boost::asio::streambuf serial_stm_buff{ 1024 };

	//If it's detected the serial hasn't read enough, the unfinished part is stored in this string
	//  so that it's used the next time data arrives
	//Doesn't need mutex because it is only used in handle_read_serial,
	//  and that function is only ran once at a time
	string unfinishedSerialString;
};

#endif