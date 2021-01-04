#include "Client.h"

#include <iostream>
#include <boost/bind.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/array.hpp>

using namespace std;
using boost::asio::ip::tcp;
using boost::asio::ip::udp;

Client::Client(boost::asio::io_context& io)
	: io_context { io },
    sock { io_context }
    //udpSock { io_context }
{
}

void Client::start(char* hostname)
{
    /*//UDP connection
    {
        udp::resolver resolver(io_context);
        udp::resolver::query query(udp::v4(), hostname, "8083");

        udp::endpoint receiver_endpoint = *resolver.resolve(query);
        udpSock.open(udp::v4());

        boost::array<char, 1> send_buf = { 0 };
        udpSock.send_to(boost::asio::buffer(send_buf), receiver_endpoint);

        boost::array<char, 128> recv_buf;
        udp::endpoint sender_endpoint;

        

        size_t len = udpSock.receive_from(
            boost::asio::buffer(recv_buf), sender_endpoint);

        auto f = receiver_endpoint.port();
        auto g = sender_endpoint.port();

        std::cout.write(recv_buf.data(), len);
    }*/

    //Starts TCP connection
    boost::asio::ip::tcp::resolver resolver { io_context };
    
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

    if (len > 0)
    {
        if (line[0] == 'q')
        {
            quit();
        }
        else
        {
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
        }
    }

    start_read_console();
}

void Client::quit()
{
    io_context.stop();
}

//boost::asio::streambuf receiveBuffer{ 1024 };

void Client::start_read_console()
{
    async_read_until(console_stm_desc, console_stm_buff, '\n', boost::bind(&Client::handle_read_input, this,
        boost::asio::placeholders::error,
        boost::asio::placeholders::bytes_transferred));
}

void Client::start_read_socket()
{
    sock.async_read_some(boost::asio::buffer(recevied, 1024),
        [this](const boost::system::error_code& err, std::size_t len)
        {
            if(err)
            {
                //If there was an error stop the program
                cout << "Connection closed" << endl;
                io_context.stop();
            }
            else if (len > 0)
            {
                std::string line(recevied);
                cout << /*"received: " << */line;

                start_read_socket();
            }
        }
    );
}