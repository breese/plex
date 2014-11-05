#ifndef PLEX_UDP_DETAIL_SOCKET_BASE_HPP
#define PLEX_UDP_DETAIL_SOCKET_BASE_HPP

#include <memory>
#include <queue>

#include <boost/asio/socket_base.hpp>
#include <boost/asio/ip/udp.hpp>

#include <plex/udp/detail/buffer.hpp>

namespace plex
{
namespace udp
{
namespace detail
{

class socket_base : public boost::asio::socket_base
{
public:
    using endpoint_type = boost::asio::ip::udp::endpoint;

    virtual ~socket_base() {}

    endpoint_type remote_endpoint() const { return remote; }

protected:
    friend class multiplexer;
    void remote_endpoint(const endpoint_type& r) { remote = r; }
    virtual void enqueue(const boost::system::error_code&,
                         std::size_t bytes_transferred,
                         std::shared_ptr<detail::buffer>) = 0;

protected:
    endpoint_type remote;
};

} // namespace detail
} // namespace udp
} // namespace plex

#endif // PLEX_UDP_DETAIL_SOCKET_BASE_HPP
