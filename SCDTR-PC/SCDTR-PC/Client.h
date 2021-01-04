#ifndef CLIENT_H
#define CLIENT_H

#include <boost/asio.hpp>

class Client
{
public:
	Client(boost::asio::io_context& io_context);
	void start(char* hostname);
private:
	boost::asio::io_context& io_context;
    

    //create copy of console stream descriptor
#ifdef _WIN32
	boost::asio::windows::stream_handle console_stm_desc{ io_context, CreateFile(L"CONIN$",
			GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED | FILE_FLAG_NO_BUFFERING, NULL) };
#else
	boost::asio::posix::stream_descriptor console_stm_desc{ io_context, ::dup(STDIN_FILENO) };
#endif

    char recevied[1024];

    //the stream to receive the console input
    boost::asio::streambuf console_stm_buff{ 1024 };

    boost::asio::ip::tcp::socket sock;
    //boost::asio::ip::udp::socket udpSock;

    /**
     * Called when user inputed something on the console. console_stm_buff contains the value
     */
    void handle_read_input(const boost::system::error_code& err, std::size_t len);
    void quit();
    void start_read_console();
    void start_read_socket();
};

#endif