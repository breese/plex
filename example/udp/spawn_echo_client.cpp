#include <iostream>
#include <string>
#include <functional>
#include <boost/asio/spawn.hpp>
#include <plex/udp/socket.hpp>

int main(int argc, char *argv[])
{
    boost::asio::io_service io;

    boost::asio::spawn
        (io,
         [&io] (boost::asio::yield_context yield)
         {
             plex::udp::endpoint local_endpoint(boost::asio::ip::udp::v4(), 0);
             plex::udp::socket socket(io, local_endpoint);

             boost::system::error_code error;
             socket.async_connect("localhost", "12346", yield[error]);
             if (!error)
             {
                 for (int i = 0; i < 5; ++i)
                 {
                     std::string input{"alpha"};
                     socket.async_send(boost::asio::buffer(input), yield);

                     unsigned char output[64];
                     auto length = socket.async_receive(boost::asio::buffer(output), yield);
                     for (auto i = 0; i < length; ++i)
                     {
                         std::cout << output[i];
                     }
                     std::cout << std::endl;
                 }
             }
         });

    io.run();
    return 0;
}
