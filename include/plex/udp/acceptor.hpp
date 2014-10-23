#ifndef PLEX_UDP_ACCEPTOR_HPP
#define PLEX_UDP_ACCEPTOR_HPP

#include <memory>
#include <boost/asio/io_service.hpp>
#include <plex/udp/detail/service.hpp>
#include <plex/udp/endpoint.hpp>
#include <plex/udp/socket.hpp>

namespace plex
{
namespace udp
{

class acceptor
    : public boost::asio::basic_io_object<detail::service>
{
public:
    using endpoint_type = plex::udp::endpoint;
    using socket_type = plex::udp::socket;

    acceptor(boost::asio::io_service& io,
             endpoint_type local_endpoint);

    template <typename AcceptHandler>
    void async_accept(socket_type& socket,
                      AcceptHandler&& handler);

    endpoint_type local_endpoint() const { return local; }

private:
    template <typename AcceptHandler>
    void process_accept(const boost::system::error_code& error,
                        socket_type& socket,
                        AcceptHandler&& handler);

private:
    std::shared_ptr<detail::multiplexer> multiplexer;
    endpoint_type local;
};

} // namespace udp
} // namespace plex

#include <cassert>
#include <utility>
#include <functional>

namespace plex
{
namespace udp
{

inline acceptor::acceptor(boost::asio::io_service& io,
                          endpoint_type local_endpoint)
    : boost::asio::basic_io_object<detail::service>(io),
      multiplexer(get_service().add(local_endpoint)) // FIXME: Should we deregister?
      // FIXME: Or create per async_accept call?
{
}

template <typename AcceptHandler>
void acceptor::async_accept(socket_type& socket,
                            AcceptHandler&& handler)
{
    assert(multiplexer);

    multiplexer->async_accept
        (socket,
         [this, &socket, handler]
         (const boost::system::error_code& error)
         {
             this->process_accept(error, socket, handler);
         });
}

template <typename AcceptHandler>
void acceptor::process_accept(const boost::system::error_code& error,
                              socket_type& socket,
                              AcceptHandler&& handler)
{
    switch (error.value())
    {
    case 0:
        {
            // FIXME: set local_endpoint?
            socket.set_multiplexer(multiplexer);
            multiplexer->add(&socket);
            boost::system::error_code success;
            handler(success);
        }
        break;

    case boost::asio::error::operation_aborted:
    default:
        handler(error);
        break;
    }
}

} // namespace udp
} // namespace plex

#endif // PLEX_UDP_ACCEPTOR_HPP
