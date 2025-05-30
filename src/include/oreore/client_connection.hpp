#ifndef OREORE_CLIENT_CONNECTION_HPP
#define OREORE_CLIENT_CONNECTION_HPP

#include <expected>
#include <oreore/ip_address.hpp>
#include <oreore/scoped_file_descriptor.hpp>
#include <string>

namespace oreore
{

    class client_connection
    {
      private:
        scoped_file_descriptor current_fd;
        ip_address             current_ip_address;
        std::string            read_buffer;
        std::string            write_buffer;
        bool                   writing_registered;

        client_connection(int target_fd, oreore::ip_address &&target_ip);

      public:
        client_connection(void)                      = delete;
        client_connection(const client_connection &) = delete;
        auto operator=(const client_connection &) -> client_connection & = delete;

        client_connection(client_connection &&other) noexcept;
        auto operator=(client_connection &&other) noexcept -> client_connection &;

        static auto make(int target_fd, oreore::ip_address &&target_ip)
            -> std::expected<client_connection, std::string>;

        [[nodiscard]] auto get_fd(void) const -> int;
        [[nodiscard]] auto get_ip_string(void) const -> std::string;
        auto               get_read_buffer(void) -> std::string &;
        auto               get_write_buffer(void) -> std::string &;
        auto               is_writing_registered(void) -> bool &;
    };

}

#endif
