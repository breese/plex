#ifndef PLEX_UDP_SOCKET_HPP
#define PLEX_UDP_SOCKET_HPP

#include <functional>
#include <memory>
#include <tuple>

#include <boost/asio/basic_io_object.hpp>
#include <boost/asio/ip/udp.hpp> // resolver

#include <plex/udp/detail/socket_base.hpp>
#include <plex/udp/detail/service.hpp>
#include <plex/udp/endpoint.hpp>

namespace plex
{
namespace udp
{
namespace detail { class multiplexer; }

class acceptor;

class socket
    : public detail::socket_base,
      public boost::asio::basic_io_object<detail::service>
{
    using resolver_type = boost::asio::ip::udp::resolver;

public:
    socket(boost::asio::io_service& io);

    socket(boost::asio::io_service& io,
           const endpoint_type& local_endpoint);

    virtual ~socket();

    // FIXME: Use asio::async_result
    template <typename ConnectHandler>
    void async_connect(const endpoint_type& remote_endpoint,
                       ConnectHandler&& handler);

    template <typename ConnectHandler>
    void async_connect(const std::string& remote_host,
                       const std::string& remote_service,
                       ConnectHandler&& handler);

    template <typename MutableBufferSequence,
              typename ReadHandler>
    void async_receive(const MutableBufferSequence& buffers,
                       ReadHandler&& handler);

    template <typename ConstBufferSequence,
              typename WriteHandler>
    void async_send(const ConstBufferSequence& buffers,
                    WriteHandler&& handler);

private:
    friend class detail::multiplexer;
    friend class acceptor;

    void set_multiplexer(std::shared_ptr<detail::multiplexer> multiplexer);

    virtual void enqueue(const boost::system::error_code& error,
                         std::size_t bytes_transferred,
                         std::shared_ptr<detail::buffer> datagram)
    {
        // FIXME: Thread-safe
        if (receive_input_queue.empty())
        {
            std::unique_ptr<receive_output_type>
                operation(new receive_output_type(error,
                                                  bytes_transferred,
                                                  datagram));
            receive_output_queue.emplace(std::move(operation));
        }
        else
        {
            auto input = std::move(receive_input_queue.front());
            receive_input_queue.pop();

            process_receive(error,
                            bytes_transferred,
                            datagram,
                            std::get<0>(*input),
                            std::get<1>(*input));
        }
    }

private:
    template <typename ErrorCode,
              typename Handler>
    void report_error(ErrorCode error,
                      Handler&& handler);

    template <typename ConnectHandler>
    void process_connect(const boost::system::error_code& error,
                         const endpoint_type&,
                         const ConnectHandler& handler);

    template <typename ConnectHandler>
    void async_next_connect(resolver_type::iterator where,
                            std::shared_ptr<resolver_type> resolver,
                            ConnectHandler&& handler);

    template <typename ConnectHandler>
    void process_next_connect(const boost::system::error_code& error,
                              resolver_type::iterator where,
                              std::shared_ptr<resolver_type> resolver,
                              ConnectHandler&& handler);

    template <typename MutableBufferSequence,
              typename ReadHandler>
    void process_receive(const boost::system::error_code& error,
                         std::size_t bytes_transferred,
                         std::shared_ptr<detail::buffer> datagram,
                         const MutableBufferSequence&,
                         ReadHandler&&);

private:
    std::shared_ptr<detail::multiplexer> multiplexer;

    using read_handler_type = std::function<void (const boost::system::error_code&, std::size_t)>;
    using receive_input_type = std::tuple<boost::asio::mutable_buffer, read_handler_type>;
    using receive_output_type = std::tuple<boost::system::error_code, std::size_t, std::shared_ptr<detail::buffer>>;
    std::queue<std::unique_ptr<receive_input_type>> receive_input_queue;
    std::queue<std::unique_ptr<receive_output_type>> receive_output_queue;
};

} // namespace udp
} // namespace plex

#include <algorithm>
#include <functional>
#include <boost/asio/error.hpp>
#include <boost/asio/io_service.hpp>
#include <plex/udp/detail/multiplexer.hpp>

namespace plex
{
namespace udp
{

inline socket::socket(boost::asio::io_service& io)
    : boost::asio::basic_io_object<detail::service>(io)
{
}

inline socket::socket(boost::asio::io_service& io,
                      const endpoint_type& local_endpoint)
    : boost::asio::basic_io_object<detail::service>(io),
      multiplexer(get_service().add(local))
{
    local = local_endpoint;
}

inline socket::~socket()
{
    if (multiplexer)
    {
        multiplexer->remove(this);
    }
    get_service().remove(local);
}

template <typename ConnectHandler>
void socket::async_connect(const endpoint_type& remote_endpoint,
                           ConnectHandler&& handler)
{
    get_io_service().dispatch
        ([this, remote_endpoint, handler]
         {
             boost::system::error_code success;
             this->process_connect(success, remote_endpoint, handler);
         });
}

template <typename ConnectHandler>
void socket::process_connect(const boost::system::error_code& error,
                             const endpoint_type& remote_endpoint,
                             const ConnectHandler& handler)
{
    // FIXME: Remove from multiplexer if already connected to different endpoint?
    if (!error)
    {
        remote = remote_endpoint;
        multiplexer->add(this);
    }
    handler(error);
}

template <typename ConnectHandler>
void socket::async_connect(const std::string& host,
                           const std::string& service,
                           ConnectHandler&& handler)
{
    std::shared_ptr<resolver_type> resolver
        = std::make_shared<resolver_type>(std::ref(get_io_service()));
    resolver_type::query query(host, service);
    resolver->async_resolve
        (query,
         [this, handler, resolver]
         (const boost::system::error_code& error,
          resolver_type::iterator where)
         {
             // Process resolve
             if (error)
             {
                 handler(error);
             }
             else
             {
                 this->async_next_connect(where, resolver, handler);
             }
         });
}

template <typename ConnectHandler>
void socket::async_next_connect(resolver_type::iterator where,
                                std::shared_ptr<resolver_type> resolver,
                                ConnectHandler&& handler)
{
    async_connect
        (*where,
         [this, where, resolver, handler]
         (const boost::system::error_code& error)
         {
             this->process_next_connect(error,
                                        where,
                                        resolver,
                                        handler);
         });
}

template <typename ConnectHandler>
void socket::process_next_connect(const boost::system::error_code& error,
                                  resolver_type::iterator where,
                                  std::shared_ptr<resolver_type> resolver,
                                  ConnectHandler&& handler)
{
    if (error)
    {
        ++where;
        if (where == resolver_type::iterator())
        {
            // No addresses left to connect to
            handler(error);
        }
        else
        {
            // Try the next address
            async_next_connect(where, resolver, handler);
        }
    }
    else
    {
        process_connect(error, *where, handler);
    }
}

template <typename MutableBufferSequence,
          typename ReadHandler>
void socket::async_receive(const MutableBufferSequence& buffers,
                           ReadHandler&& handler)
{
    if (!multiplexer)
    {
        report_error(boost::asio::error::not_connected,
                     std::forward<ReadHandler>(handler));
        return;
    }

    if (receive_output_queue.empty())
    {
        std::unique_ptr<receive_input_type> operation(new receive_input_type(buffers,
                                                                             std::move(handler)));
        receive_input_queue.emplace(std::move(operation));

        multiplexer->start_receive();
    }
    else
    {
        get_io_service().dispatch
            ([this, &buffers, handler] ()
             {
                 // FIXME: Thread-safe
                 auto output = std::move(this->receive_output_queue.front());
                 this->receive_output_queue.pop();
                 this->process_receive(std::get<0>(*output),
                                       std::get<1>(*output),
                                       std::get<2>(*output),
                                       buffers,
                                       handler);
             });
    }
}

template <typename MutableBufferSequence,
          typename ReadHandler>
void socket::process_receive(const boost::system::error_code& error,
                             std::size_t bytes_transferred,
                             std::shared_ptr<detail::buffer> datagram,
                             const MutableBufferSequence& buffers,
                             ReadHandler&& handler)
{
    if (!error)
    {
        boost::asio::buffer_copy(buffers,
                                 boost::asio::buffer(*datagram),
                                 bytes_transferred);
    }
    handler(error, bytes_transferred);
}

template <typename ConstBufferSequence,
          typename WriteHandler>
void socket::async_send(const ConstBufferSequence& buffers,
                        WriteHandler&& handler)
{
    if (!multiplexer)
    {
        report_error(boost::asio::error::not_connected,
                     std::forward<WriteHandler>(handler));
        return;
    }

    multiplexer->async_send_to
        (buffers,
         remote,
         [handler] (const boost::system::error_code& error,
                    std::size_t bytes_transferred)
         {
             // Process send
             handler(error, bytes_transferred);
         });
}

template <typename ErrorCode,
          typename Handler>
void socket::report_error(ErrorCode error,
                          Handler&& handler)
{
    assert(error);

    get_io_service().post
        ([error, handler]
         {
             handler(boost::asio::error::make_error_code(error), 0);
         });
}

inline void socket::set_multiplexer(std::shared_ptr<detail::multiplexer> m)
{
    multiplexer = m;
}

} // namespace udp
} // namespace plex

#endif // PLEX_UDP_SOCKET_HPP
