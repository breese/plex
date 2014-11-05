#ifndef PLEX_UDP_DETAIL_MULTIPLEXER_HPP
#define PLEX_UDP_DETAIL_MULTIPLEXER_HPP

#include <atomic>
#include <memory>
#include <functional>
#include <queue>
#include <map>
#include <queue>
#include <tuple>

#include <boost/asio/placeholders.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/udp.hpp>

#include <plex/udp/detail/buffer.hpp>

namespace plex
{
namespace udp
{
namespace detail
{

class socket_base;

// FIXME: Thread-safety (strand?)

class multiplexer : public std::enable_shared_from_this<multiplexer>
{
public:
    using protocol_type = boost::asio::ip::udp;
    using next_layer_type = protocol_type::socket;
    using endpoint_type = protocol_type::endpoint;
    using buffer_type = detail::buffer;

    template <typename... Types>
    static std::shared_ptr<multiplexer> create(Types&&...);

    ~multiplexer();

    void add(socket_base *);
    void remove(socket_base *);

    template <typename SocketType,
              typename AcceptHandler>
    void async_accept(SocketType&,
                      AcceptHandler&& handler);

    template <typename ConstBufferSequence,
              typename CompletionToken>
    typename boost::asio::async_result<
        typename boost::asio::handler_type<CompletionToken,
                                           void(boost::system::error_code, std::size_t)>::type
        >::type
    async_send_to(const ConstBufferSequence& buffers,
                       const endpoint_type& endpoint,
                       CompletionToken&& token);

    void start_receive();

    const next_layer_type& next_layer() const;

private:
    multiplexer(boost::asio::io_service& io,
                endpoint_type local_endpoint);

    void do_start_receive();

    void process_receive(const boost::system::error_code&,
                         std::size_t bytes_transferred,
                         std::shared_ptr<buffer_type>,
                         const endpoint_type&);

    template <typename AcceptHandler>
    void process_accept(const boost::system::error_code& error,
                        std::size_t bytes_transferred,
                        socket_base *,
                        std::shared_ptr<buffer_type> datagram,
                        const endpoint_type& remote_endpoint,
                        AcceptHandler&& handler);

private:
    next_layer_type socket;

    using socket_map = std::map<endpoint_type, socket_base *>;
    socket_map sockets;

    std::atomic<int> receive_calls;

    // FIXME: Move to acceptor class
    // FIXME: Bounded queue with pending accept requests? (like listen() backlog)
    using accept_handler_type = std::function<void (const boost::system::error_code&)>;
    using accept_input_type = std::tuple<socket_base *, accept_handler_type>;
    std::queue<std::unique_ptr<accept_input_type>> acceptor_queue;
};

} // namespace detail
} // namespace udp
} // namespace plex

#include <cassert>
#include <utility>
#include <boost/asio/buffer.hpp>
#include <plex/udp/detail/socket_base.hpp>

namespace plex
{
namespace udp
{
namespace detail
{

template <typename... Types>
std::shared_ptr<multiplexer> multiplexer::create(Types&&... args)
{
    std::shared_ptr<multiplexer> self(new multiplexer{std::forward<Types>(args)...});
    return self;
}

inline multiplexer::multiplexer(boost::asio::io_service& io,
                                endpoint_type local_endpoint)
    : socket(io, local_endpoint),
      receive_calls(0)
{
}

inline multiplexer::~multiplexer()
{
    assert(sockets.empty());

    // FIXME: Clean up
}

inline void multiplexer::add(socket_base *socket)
{
    assert(socket);

    sockets.insert(socket_map::value_type(socket->remote_endpoint(), socket));
}

inline void multiplexer::remove(socket_base *socket)
{
    assert(socket);

    sockets.erase(socket->remote_endpoint());
    // FIXME: Prune request queues
}

template <typename SocketType,
          typename AcceptHandler>
void multiplexer::async_accept(SocketType& socket,
                               AcceptHandler&& handler)
{
    std::unique_ptr<accept_input_type> operation(new accept_input_type(&socket,
                                                                       std::move(handler)));
    acceptor_queue.emplace(std::move(operation));

    if (receive_calls++ == 0)
    {
        do_start_receive();
    }
}

template <typename AcceptHandler>
void multiplexer::process_accept(const boost::system::error_code& error,
                                 std::size_t bytes_transferred,
                                 socket_base *socket,
                                 std::shared_ptr<buffer_type> datagram,
                                 const endpoint_type& current_remote_endpoint,
                                 AcceptHandler&& handler)
{
    if (!error)
    {
        socket->remote_endpoint(current_remote_endpoint);
        // Queue datagram for later use
        socket->enqueue(error, bytes_transferred, datagram);
    }
    handler(error);
}

template <typename ConstBufferSequence,
          typename CompletionToken>
typename boost::asio::async_result<
    typename boost::asio::handler_type<CompletionToken,
                                       void(boost::system::error_code, std::size_t)>::type
    >::type
multiplexer::async_send_to(const ConstBufferSequence& buffers,
                           const endpoint_type& endpoint,
                           CompletionToken&& token)
{
    // FIXME: Congestion control
    typename boost::asio::handler_type<CompletionToken,
                              void(boost::system::error_code, std::size_t)>::type handler(std::forward<CompletionToken>(token));
    boost::asio::async_result<decltype(handler)> result(handler);
    socket.async_send_to(buffers,
                         endpoint,
                         std::forward<decltype(handler)>(handler));
    return result.get();
}

inline void multiplexer::start_receive()
{
    if (receive_calls++ == 0)
    {
        do_start_receive();
    }
}

inline void multiplexer::do_start_receive()
{
    auto remote_endpoint = std::make_shared<endpoint_type>();
    // FIXME: Improve buffer management
    auto datagram = std::make_shared<buffer_type>(1400); // FIXME: size
    auto self(shared_from_this());
    socket.async_receive_from
        (boost::asio::buffer(datagram->data(), datagram->capacity()),
         *remote_endpoint,
         [self, datagram, remote_endpoint]
         (const boost::system::error_code& error, std::size_t size) mutable
         {
             self->process_receive(error, size, datagram, *remote_endpoint);
         });
}

inline
void multiplexer::process_receive(const boost::system::error_code& error,
                                  std::size_t bytes_transferred,
                                  std::shared_ptr<buffer_type> datagram,
                                  const endpoint_type& remote_endpoint)
{
    // FIXME: Handle error == operation_aborted

    if (--receive_calls  > 0)
    {
        do_start_receive();
    }

    auto recipient = sockets.find(remote_endpoint);
    if (recipient == sockets.end())
    {
        // Unknown endpoint
        if (!acceptor_queue.empty())
        {
            auto input = std::move(acceptor_queue.front());
            acceptor_queue.pop();
            process_accept(error,
                           bytes_transferred,
                           std::get<0>(*input),
                           datagram,
                           remote_endpoint,
                           std::get<1>(*input));
        }
        // FIXME: else enqueue or ignore datagram?
        // FIXME: Collect datagrams from different remote_endpoints
        // FIXME: Call start_receive()?
    }
    else
    {
        // Enqueue datagram on socket
        (*recipient).second->enqueue(error, bytes_transferred, datagram);
    }
}

inline const multiplexer::next_layer_type& multiplexer::next_layer() const
{
    return socket;
}

} // namespace detail
} // namespace udp
} // namespace plex

#endif // PLEX_UDP_DETAIL_MULTIPLEXER_HPP
