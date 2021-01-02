#include <ctime>
#include <iostream>
#include <string>
#include <boost/bind/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/asio.hpp>
#include <memory>
//#include <unistd.h>

#include "program.h"
#include "commands.h"
#include "Client.h"

using boost::asio::ip::tcp;
using namespace std;

bool server = false;

// IO for the serial and console
boost::asio::io_context io_context;

int runClient(char* hostname)
{
    Client client{ io_context };

    try
    {
        client.start(hostname);

        io_context.run();

        return 0;
    }
    catch (std::exception& e)
    {
        std::cerr << e.what() << std::endl;
        return 1;
    }

    
}

void run_service()
{
    io_context.run();
}

int runServer(char* serialStr)
{
    try
    {
        std::shared_ptr<Program> program = std::make_shared<Program>(io_context);
        Commands::program = program;

        program->start(serialStr);

        thread thread1{ run_service };
        thread thread2{ run_service };

        thread1.join();
        thread2.join();
    }
    catch (std::exception& e)
    {
        std::cerr << e.what() << std::endl;
        return 1;
    }

    return 0;
}

int main(int argc, char *argv[])
{
    bool parametersError = false;
    if (argc < 3)
    {
        parametersError = true;
    }
    else if (strcmp(argv[1], "--server") == 0)
    {
        server = true;
        //argv[2] Contains the string of where the serial port is ex: "/dev/ttyACM0" or "COM2"
        runServer(argv[2]);
    }
    else if (strcmp(argv[1], "--client") == 0)
    {
        //argv[2] Contains the ip address
        runClient(argv[2]);
    }
    else
    {
        parametersError = true;
    }

    if (parametersError)
    {
        cout << "Usage: --server <serial_port> or --client <server_hostname>" << endl;
        return 1;
    }

    return 0;
}