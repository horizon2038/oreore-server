#ifndef OREORE_SERVER_HPP
#define OREORE_SERVER_HPP

#include <oreore/client_connection.hpp>
#include <oreore/message.hpp>
#include <oreore/scoped_file_descriptor.hpp>

#include <expected>
#include <map>
#include <mutex>
#include <vector>

namespace oreore
{

    class server
    {
      private:
        scoped_file_descriptor epoll_file_descriptor;
        scoped_file_descriptor server_file_descriptor;

        std::vector<message>             messages;
        std::mutex                       messages_mutex;
        uintmax_t                        next_message_id;
        std::map<int, client_connection> client_connections;

        server(scoped_file_descriptor &&epoll_fd, scoped_file_descriptor &&server_fd);

        auto register_descriptor(int fd, uint32_t events)
            -> std::expected<void, std::string>;
        auto modify_descriptor(int fd, uint32_t new_events)
            -> std::expected<void, std::string>;
        auto unregister_descriptor(int fd) -> std::expected<void, std::string>;

        auto close_client(int client_fd, const char *reason) -> void;
        auto accept_new_connections(void) -> void;
        auto handle_client_read(client_connection &client) -> void;
        auto handle_client_write(client_connection &client) -> void;
        auto queue_data_for_send(client_connection &client, std::string data_to_send)
            -> void;
        auto process_client_command(
            client_connection &client,
            const std::string &command_line
        ) -> void;

      public:
        server(const server &)                     = delete;
        auto operator=(const server &) -> server & = delete;

        server(server &&) noexcept;
        auto operator=(server &&) noexcept -> server &;

        ~server(void);

        static auto make(uint16_t port, int backlog)
            -> std::expected<server, std::string>;
        auto run(void) -> void;
    };

    auto make_socket_non_blocking(int socket_fd)
        -> std::expected<void, std::string>;

}

#endif
