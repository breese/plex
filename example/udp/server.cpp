#include <iostream>
#include <memory>
#include <functional>
#include <vector>
#include <boost/asio.hpp>
#include <plex/udp/acceptor.hpp>
#include <plex/udp/socket.hpp>

class server
{
    using message_type = std::vector<char>;

public:
    server(boost::asio::io_service& io,
           const boost::asio::ip::udp::endpoint& local_endpoint)
        : acceptor(io, local_endpoint),
          socket(io) // FIXME: Pass local_endpoint?
    {
        do_accept();
    }

    void do_accept()
    {
        acceptor.async_accept(socket,
                              std::bind(&server::on_accept,
                                        this,
                                        std::placeholders::_1));
    }

    void on_accept(const boost::system::error_code& error)
    {
        std::cout << __FUNCTION__ << "(" << error.message() << ")" << std::endl;
        if (!error)
        {
            do_receive();
            do_accept();
        }
    }

    void do_receive()
    {
        std::shared_ptr<message_type> message = std::make_shared<message_type>(1400);
        socket.async_receive(boost::asio::buffer(*message),
                             std::bind(&server::on_receive,
                                       this,
                                       std::placeholders::_1,
                                       std::placeholders::_2,
                                       message));
    }

private:
    void on_receive(const boost::system::error_code& error,
                    std::size_t message_size,
                    std::shared_ptr<message_type> message)
    {
        std::cout << __FUNCTION__ << "(" << error.message() << ", " << message_size << ")" << std::endl;
        for (int i = 0; i < message_size; ++i)
        {
            std::cout << (*message)[i];
        }
        std::cout << std::endl;
        do_receive();
    }

private:
    plex::udp::acceptor acceptor;
    plex::udp::socket socket;
};

int main(int argc, char *argv[])
{
    boost::asio::io_service io;
    boost::asio::ip::udp::endpoint endpoint(boost::asio::ip::udp::v4(), 12346);
    server s(io, endpoint);
    io.run();
    return 0;
}
