/*#include "Server.h"

#include <boost/bind/bind.hpp>

using boost::asio::ip::tcp;
using namespace std;

Server::Server(boost::asio::io_context& io_context)
        : io_context(io_context),
    acceptor(io_context, tcp::endpoint(tcp::v4(), TCP_PORT))
{
    start_accept();
}

void Server::start_accept()
{
    //The connection to a client
    //Has to be a pointer because it is passed to the handle_accept and here it goes out of scope
    shared_ptr<ServerConnection> session = make_shared<ServerConnection>(io_context);

    //The socket for a new connection to a client
    //shared_ptr<tcp::socket> socket = make_shared<tcp::socket>(tcp::socket{ io_context });

    acceptor.async_accept(session->socket,
        boost::bind(&Server::handle_accept, this, session,
            boost::asio::placeholders::error));
}*/