#ifndef PLEX_UDP_ENDPOINT_HPP
#define PLEX_UDP_ENDPOINT_HPP

#include <boost/asio/ip/udp.hpp>

namespace plex
{
namespace udp
{

using endpoint = boost::asio::ip::udp::endpoint;

} // namespace udp
} // namespace plex

#endif // PLEX_UDP_ENDPOINT_HPP
