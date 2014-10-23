#ifndef PLEX_UDP_DETAIL_BUFFER_HPP
#define PLEX_UDP_DETAIL_BUFFER_HPP

#include <algorithm>
#include <vector>

namespace plex
{
namespace udp
{
namespace detail
{

#if 1 // FIXME: Move typedef
using buffer = std::vector<char>;

#else

class buffer
{
    using storage_type = std::vector<char>;

public:
    buffer(std::size_t capacity)
        : storage(capacity),
          amount(0)
    {
    }

    bool empty() const { return amount == 0; }
    void size(std::size_t s)
    {
        amount = std::min(s, storage.size());
    }

    storage_type::pointer data() { return storage.data(); }
    storage_type::const_pointer data() const { return storage.data(); }
    std::size_t size() const { return amount; }
    std::size_t capacity() const { return storage.size(); }

private:
    storage_type storage;
    std::size_t amount; // Invariant: amount <= storage.size()
};

#endif

} // namespace detail
} // namespace udp
} // namespace plex

#include <boost/asio/buffer.hpp>

namespace boost
{
namespace asio
{

inline mutable_buffers_1 buffer(plex::udp::detail::buffer& data)
{
    return mutable_buffers_1(data.empty() ? nullptr : data.data(),
                             data.size() * sizeof(char));
}

inline const_buffers_1 buffer(const plex::udp::detail::buffer& data)
{
    return const_buffers_1(data.empty() ? nullptr : data.data(),
                           data.size() * sizeof(char));
}

} // namespace asio
} // namespace boost

#endif // PLEX_UDP_DETAIL_BUFFER_HPP
