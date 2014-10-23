#include <string>
#include <vector>
#include <functional>
#include <boost/asio/connect.hpp>
#include <plex/udp/socket.hpp>

class client
{
    using message_type = std::vector<char>;

public:
    client(boost::asio::io_service& io,
           const std::string& host,
           const std::string& service)
        : socket(io,
                 // FIXME: Does this work as local_endpoint?
                 boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), 0)),
          counter(4)
    {
        socket.async_connect(host,
                             service,
                             std::bind(&client::on_connect,
                                       this,
                                       std::placeholders::_1));
    }

    void on_connect(const boost::system::error_code& error)
    {
        std::cout << __FUNCTION__ << "(" << error.message() << ")" << std::endl;

        if (!error)
        {
            do_send("alpha");
        }
    }

    void do_send(const std::string& data)
    {
        std::shared_ptr<message_type> message = std::make_shared<message_type>(data.begin(), data.end());
        socket.async_send(boost::asio::buffer(*message),
                          std::bind(&client::on_send,
                                    this,
                                    std::placeholders::_1,
                                    std::placeholders::_2,
                                    message));
    }

    void on_send(const boost::system::error_code& error,
                 std::size_t message_size,
                 std::shared_ptr<message_type> message)
    {
        std::cout << __FUNCTION__ << "(" << error.message() << ")" << std::endl;

        if (!error)
        {
            if (--counter > 0)
            {
                do_send("bravo");
            }
        }
    }

private:
    plex::udp::socket socket;
    int counter;
};

int main(int argc, char *argv[])
{
    boost::asio::io_service io;
    client c(io, "localhost", "12346");
    io.run();
    return 0;
}
