#include <string>
#include <vector>
#include <functional>
#include <thread>
#include <boost/asio/buffer.hpp>
#include <boost/asio/use_future.hpp>
#include <plex/udp/socket.hpp>

void send_packets(boost::asio::io_service& io)
{
    plex::udp::endpoint local_endpoint(boost::asio::ip::udp::v4(), 0);
    plex::udp::socket socket(io, local_endpoint);

    try
    {
        socket.async_connect("localhost", "12346", boost::asio::use_future).get();
        std::cout << "Connected with Success" << std::endl;

        for (int i = 0; i < 5; ++i)
        {
            std::string input{"alpha"};
            socket.async_send(boost::asio::buffer(input), boost::asio::use_future).get();

            unsigned char output[64];
            auto length = socket.async_receive(boost::asio::buffer(output), boost::asio::use_future).get();
            for (auto i = 0; i < length; ++i)
            {
                std::cout << output[i];
            }
            std::cout << std::endl;
        }
    }
    catch (const std::exception& ex)
    {
        std::cout << "Error: " << ex.what() << std::endl;
    }
}

int main(int argc, char *argv[])
{
    boost::asio::io_service io;
    boost::asio::io_service::work work(io);

    std::thread io_thread([&io] { io.run(); });

    send_packets(io);
    io.stop();
    io_thread.join();
    return 0;
}
