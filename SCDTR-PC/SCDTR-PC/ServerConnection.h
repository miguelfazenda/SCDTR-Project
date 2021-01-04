#ifndef _SERVER_CONNECTION_H
#define _SERVER_CONNECTION_H

#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>

class Server;

class ServerConnection : public std::enable_shared_from_this<ServerConnection>
{
public:
	ServerConnection(boost::asio::io_context& io_context, std::weak_ptr<Server> server);
	void start();
	void sendMessage(std::string msg);

	//char streamingActive = 0;

	std::weak_ptr<Server> server;

	char recevied[1024];


	//std::shared_ptr<boost::asio::ip::tcp::socket> socket;
	boost::asio::ip::tcp::socket socket;

	boost::asio::streambuf receiveBuffer{ 1024 };
private:
	void start_receive_read();
};

#endif // !_SERVER_CONNECTION_H