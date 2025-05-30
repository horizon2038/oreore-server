#include <oreore/client_connection.hpp>

namespace oreore
{
    client_connection::client_connection(int target_fd, ip_address &&target_ip)
        : current_fd(target_fd)
        , current_ip_address(std::move(target_ip))
        , writing_registered(false)
    {
    }

    client_connection::client_connection(client_connection &&other) noexcept
        : current_fd(other.current_fd.release())
        , // Take ownership of the fd
        current_ip_address(std::move(other.current_ip_address))
        , read_buffer(std::move(other.read_buffer))
        , write_buffer(std::move(other.write_buffer))
        , writing_registered(other.writing_registered)
    {
        other.writing_registered = false;
    }

    auto client_connection::operator=(client_connection &&other) noexcept
        -> client_connection &
    {
        if (this != &other)
        {
            current_fd = std::move(other.current_fd); // scoped_fd move
                                                      // assignment handles
                                                      // closing old fd
            current_ip_address       = std::move(other.current_ip_address);
            read_buffer              = std::move(other.read_buffer);
            write_buffer             = std::move(other.write_buffer);
            writing_registered       = other.writing_registered;
            other.writing_registered = false;
        }
        return *this;
    }

    auto client_connection::make(int target_fd, ip_address &&target_ip)
        -> std::expected<client_connection, std::string>
    {
        if (target_fd < 0)
        {
            return std::unexpected("client_connection::make error: Invalid "
                                   "file descriptor.");
        }
        if (!target_ip.get_raw().has_value()
            || !target_ip.get_string().has_value())
        {
            return std::unexpected("client_connection::make error: Provided "
                                   "ip_address is not fully initialized.");
        }
        return client_connection(target_fd, std::move(target_ip));
    }

    auto client_connection::get_fd(void) const -> int
    {
        return current_fd.get();
    }

    auto client_connection::get_ip_string(void) const -> std::string
    {
        return current_ip_address.get_string().value_or("Unknown IP");
    }

    auto client_connection::get_read_buffer(void) -> std::string &
    {
        return read_buffer;
    }

    auto client_connection::get_write_buffer(void) -> std::string &
    {
        return write_buffer;
    }

    auto client_connection::is_writing_registered(void) -> bool &
    {
        return writing_registered;
    }

}
