#include "Client.h"

#include <iostream>
#include <boost/bind.hpp>
#include <boost/asio/buffer.hpp>

using namespace std;
using boost::asio::ip::tcp;

Client::Client(boost::asio::io_context& io)
	: io_context { io },
    sock { io_context }
{
}

void Client::start(char* hostname)
{
    boost::asio::ip::tcp::resolver resolver { io_context };
    
    //Connects to an endpoint
    tcp::resolver::query query(hostname, "8082");
    tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
    tcp::resolver::iterator end;
    boost::system::error_code error = boost::asio::error::host_not_found;
    while (error && endpoint_iterator != end)
    {
        sock.close();
        sock.connect(*endpoint_iterator++, error);
    }
    if (error)
        throw boost::system::system_error(error);


    start_read_socket();
    start_read_console();
}

void Client::handle_read_input(const boost::system::error_code& err, std::size_t len)
{
    std::istream input(&console_stm_buff);
    std::string line;
    getline(input, line, '@');

    //Send message
    sock.async_write_some(boost::asio::buffer(line.c_str(), line.size() + 1),
        [this](const boost::system::error_code& err, size_t len)
        {
            if (err)
            {
                //If there was an error stop the program
                cout << "Error writing message" << endl;
                io_context.stop();
            }
        }
    );

    start_read_console();
}

//boost::asio::streambuf receiveBuffer{ 1024 };

void Client::start_read_console()
{
    async_read_until(console_stm_desc, console_stm_buff, '\n', boost::bind(&Client::handle_read_input, this,
        boost::asio::placeholders::error,
        boost::asio::placeholders::bytes_transferred));
}
char dataaa[1024];

void Client::start_read_socket()
{
    sock.async_read_some(boost::asio::buffer(dataaa, 1024),
        [this](const boost::system::error_code& err, std::size_t len)
        {
            if(err)
            {
                //If there was an error stop the program
                cout << "Error reading message" << endl;
                io_context.stop();
            }
            else if (len > 0)
            {
                std::string line(dataaa);
                cout << /*"received: " << */line;

                start_read_socket();
            }
        }
    );
}