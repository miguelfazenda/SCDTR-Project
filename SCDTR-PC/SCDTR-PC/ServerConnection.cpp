#include "ServerConnection.h"

#include <boost/bind/bind.hpp>
#include <boost/asio.hpp>
#include <boost/asio/buffer.hpp>
#include <iostream>
#include "commands.h"
#include "command.h"
#include "Server.h"

using namespace boost::asio;
using boost::asio::ip::tcp;
using namespace std;

ServerConnection::ServerConnection(boost::asio::io_context& io_context, weak_ptr<Server> server)
	: socket{ io_context } //Initializes the socket
{
	this->server = server;
}

ServerConnection::~ServerConnection()
{
	cout << "A client has disconnected" << endl;
}

void ServerConnection::start()
{
	sendMessage("Welcome!\n");

	//Reads the next data
	start_receive_read();
}

void ServerConnection::sendMessage(std::string msg)
{
	async_write(socket, boost::asio::buffer(msg.c_str(), msg.size()+1),
		[this](const boost::system::error_code& err, std::size_t len)
		{
			if (err)
			{
				//Removes client from the server
				if (shared_ptr<Server> server_ptr = server.lock()) {
					server_ptr->removeClientSession(shared_from_this());
				}
			}
			else
			{
				cout << "written!" << endl;
			}
		}
	);
}


//Reads the next data
void ServerConnection::start_receive_read()
{
	socket.async_read_some(boost::asio::buffer(recevied, 1024),
		[this](const boost::system::error_code& err, std::size_t len)
	{
		if (err)
		{
			//Removes client from the server
			if (shared_ptr<Server> server_ptr = server.lock()) {
				server_ptr->removeClientSession(shared_from_this());
			}
		}

		if (len > 0)
		{
			std::string line(recevied);
			
			//Writes the command response here if it's to then be redirected to the client
			std::ostringstream textOutputForClient;

			Command command = Commands::interpretCommand(line, textOutputForClient, shared_from_this());
			if (command.cmd != '\0')
			{
				//Means the a command was read from the line (invalid command will have cmd=='\0')

				//Gets a shared_ptr to the server (since this class stores a weak_ptr)
				auto server_ptr = server.lock();
				if (server_ptr)
					server_ptr->addCommandToQueue(command, shared_from_this());
				else
					cout << "Error!" << endl;
			}

			//Sends the text written by the Commands::interpretCommand function to the client
			if (textOutputForClient.str().length() > 0)
			{
				sendMessage(textOutputForClient.str());
			}

			start_receive_read();
		}
	});
}
