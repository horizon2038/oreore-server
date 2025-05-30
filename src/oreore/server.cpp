#include <oreore/server.hpp>

#include <algorithm>
#include <charconv>
#include <fcntl.h>
#include <iostream>
#include <sstream>
#include <sys/epoll.h>
#include <unistd.h>

namespace oreore
{

    // --- Private Constructor ---
    server::server(scoped_file_descriptor &&epoll_fd, scoped_file_descriptor &&server_fd)
        : epoll_file_descriptor(std::move(epoll_fd))
        , server_file_descriptor(std::move(server_fd))
        , next_message_id(0)
    {
    }

    // --- Destructor ---
    server::~server(void)
    {
        if (epoll_file_descriptor.get() != -1)
        {
            client_connections.clear();
        }
    }

    // --- Move Constructor & Assignment (custom implementation for std::mutex)
    // ---
    server::server(server &&other) noexcept
        : epoll_file_descriptor(std::move(other.epoll_file_descriptor))
        , server_file_descriptor(std::move(other.server_file_descriptor))
        , messages(std::move(other.messages))
        , next_message_id(other.next_message_id)
        , client_connections(std::move(other.client_connections))
    {
        // messages_mutex is default-initialized in the new object
        other.next_message_id = 0;
    }

    auto server::operator=(server &&other) noexcept -> server &
    {
        if (this == &other)
        {
            return *this;
        }
        epoll_file_descriptor  = std::move(other.epoll_file_descriptor);
        server_file_descriptor = std::move(other.server_file_descriptor);
        client_connections     = std::move(other.client_connections);

        std::lock_guard<std::mutex> lock_this(messages_mutex); // Lock self
                                                               // before
                                                               // modifying
        messages              = std::move(other.messages);
        next_message_id       = other.next_message_id;

        other.next_message_id = 0;

        return *this;
    }

    // --- Epoll Helper Methods ---
    auto server::register_descriptor(int fd, uint32_t events)
        -> std::expected<void, std::string>
    {
        epoll_event event {};
        event.data.fd = fd;
        event.events  = events;
        if (epoll_ctl(epoll_file_descriptor.get(), EPOLL_CTL_ADD, fd, &event)
            == -1)
        {
            return std::unexpected(make_errno_message(
                "epoll_ctl ADD failed for fd " + std::to_string(fd)
            ));
        }
        return {};
    }

    auto server::modify_descriptor(int fd, uint32_t new_events)
        -> std::expected<void, std::string>
    {
        epoll_event event {};
        event.data.fd = fd;
        event.events  = new_events;
        if (epoll_ctl(epoll_file_descriptor.get(), EPOLL_CTL_MOD, fd, &event)
            == -1)
        {
            return std::unexpected(make_errno_message(
                "epoll_ctl MOD failed for fd " + std::to_string(fd)
            ));
        }
        return {};
    }

    auto server::unregister_descriptor(int fd) -> std::expected<void, std::string>
    {
        if (epoll_ctl(epoll_file_descriptor.get(), EPOLL_CTL_DEL, fd, nullptr) == -1
            && errno != ENOENT)
        {
            return std::unexpected(make_errno_message(
                "epoll_ctl DEL failed for fd " + std::to_string(fd)
            ));
        }
        return {};
    }

    auto server::close_client(int client_fd, const char *reason) -> void
    {
        auto client_iterator = client_connections.find(client_fd);
        if (client_iterator == client_connections.end())
        {
            return;
        }

        if (reason)
        {
            std::cout << "Closing client "
                      << client_iterator->second.get_ip_string() << " (socket "
                      << client_fd << "): " << reason << std::endl;
        }
        unregister_descriptor(client_fd);
        client_connections.erase(client_iterator);
    }

    // --- Event Handlers ---
    auto server::accept_new_connections(void) -> void
    {
        while (true)
        {
            sockaddr_in client_address {};
            socklen_t   client_len    = sizeof(client_address);
            int         client_fd_val = accept(
                server_file_descriptor.get(),
                (struct sockaddr *)&client_address,
                &client_len
            );

            if (client_fd_val == -1)
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    break;
                perror("accept error");
                break;
            }

            scoped_file_descriptor scoped_client_fd(client_fd_val
            ); // RAII for the accepted fd

            auto non_blocking_res
                = make_socket_non_blocking(scoped_client_fd.get());
            if (!non_blocking_res)
            {
                std::cerr << "Failed to make socket non-blocking for fd "
                          << scoped_client_fd.get() << ": "
                          << non_blocking_res.error() << std::endl;
                continue; // Try next accept
            }

            auto ip_expected
                = ip_address::make(ntohl(client_address.sin_addr.s_addr));
            if (!ip_expected)
            {
                std::cerr << "Failed to create ip_address for fd "
                          << scoped_client_fd.get() << ": "
                          << ip_expected.error() << std::endl;
                continue; // Try next accept
            }

            // Release fd from scoped_client_fd as client_connection will take
            // ownership
            auto conn_expected = client_connection::make(
                scoped_client_fd.release(),
                std::move(ip_expected.value())
            );
            if (!conn_expected)
            {
                std::cerr << "Failed to create client_connection: "
                          << conn_expected.error() << std::endl;
            }

            client_connection new_conn = std::move(conn_expected.value());
            int new_client_fd_val = new_conn.get_fd(); // fd is now owned by
                                                       // new_conn's internal
                                                       // scoped_fd

            auto registration_result
                = register_descriptor(new_client_fd_val, EPOLLIN | EPOLLET);
            if (!registration_result)
            {
                std::cerr << "Failed to register client fd " << new_client_fd_val
                          << " with epoll: " << registration_result.error()
                          << std::endl;
                // new_conn goes out of scope, its scoped_fd closes the socket.
                continue; // Try next accept
            }

            std::cout << "Accepted new connection from "
                      << new_conn.get_ip_string() << " on socket "
                      << new_client_fd_val << std::endl;
            client_connections.emplace(new_client_fd_val, std::move(new_conn));
        }
    }

    auto server::process_client_command(
        client_connection &client,
        const std::string &command_line
    ) -> void
    {
        std::cout << "Processing for " << client.get_ip_string() << " (socket "
                  << client.get_fd() << "): " << command_line << std::endl;
        std::string        response_str;
        std::istringstream iss_cmd(command_line);
        std::string        command_token;
        iss_cmd >> command_token;

        if (command_token == "POST")
        {
            std::string message_text;
            if (command_line.rfind("POST ", 0) == 0)
            {
                message_text = command_line.substr(5);
            }
            else
            {
                response_str = "ERR: Invalid POST format. Usage: POST "
                               "<message>\n";
                queue_data_for_send(client, std::move(response_str));
                return;
            }
            std::lock_guard<std::mutex> lock(messages_mutex);
            uintmax_t                   current_id = next_message_id++;
            messages.push_back(
                { current_id, message_text, client.get_ip_string(), "" }
            );
            response_str = "OK: Message " + std::to_string(current_id)
                         + " posted.\n";
        }
        else if (command_token == "GET")
        {
            std::lock_guard<std::mutex> lock(messages_mutex);
            if (messages.empty())
            {
                response_str = "Stack is empty.\n";
            }
            else
            {
                std::ostringstream oss_resp;
                for (const auto &msg_item : messages)
                {
                    oss_resp
                        << "ID: " << msg_item.id
                        << ", From: " << msg_item.sender_ip << ", Reaction: ["
                        << (msg_item.reaction.empty() ? "" : msg_item.reaction)
                        << "]"
                        << ", Msg: \"" << msg_item.text << "\"\n";
                }
                response_str = oss_resp.str();
            }
        }
        else if (command_token == "HAPPY" || command_token == "SAD")
        {
            std::string id_str_cmd;
            iss_cmd >> id_str_cmd;
            if (id_str_cmd.empty())
            {
                response_str = "ERR: Message ID not provided for "
                             + command_token + ".\n";
            }
            else
            {
                uintmax_t message_id_val;
                auto [ptr_cmd, ec_cmd] = std::from_chars(
                    id_str_cmd.data(),
                    id_str_cmd.data() + id_str_cmd.size(),
                    message_id_val
                );
                if (ec_cmd == std::errc()
                    && ptr_cmd == id_str_cmd.data() + id_str_cmd.size())
                {
                    std::lock_guard<std::mutex> lock(messages_mutex);
                    auto                        it_msg = std::find_if(
                        messages.begin(),
                        messages.end(),
                        [message_id_val](const message &m)
                        {
                            return m.id == message_id_val;
                        }
                    );
                    if (it_msg != messages.end())
                    {
                        it_msg->reaction = command_token;
                        response_str     = "OK: Reaction set for message "
                                     + std::to_string(message_id_val) + ".\n";
                    }
                    else
                    {
                        response_str
                            = "ERR: Message ID "
                            + std::to_string(message_id_val) + " not found.\n";
                    }
                }
                else
                {
                    response_str = "ERR: Invalid message ID format '"
                                 + id_str_cmd + "'. Must be an integer.\n";
                }
            }
        }
        else
        {
            if (!command_token.empty())
            {
                response_str = "ERR: Unknown command '" + command_token + "'.\n";
            }
        }

        if (!response_str.empty())
        {
            queue_data_for_send(client, std::move(response_str));
        }
    }

    auto server::queue_data_for_send(
        client_connection &client,
        std::string        data_to_send
    ) -> void
    {
        client.get_write_buffer().append(std::move(data_to_send));

        if (!client.is_writing_registered() && !client.get_write_buffer().empty())
        {
            ssize_t sent_bytes = send(
                client.get_fd(),
                client.get_write_buffer().data(),
                client.get_write_buffer().length(),
                MSG_NOSIGNAL
            );
            if (sent_bytes >= 0)
            {
                client.get_write_buffer().erase(0, sent_bytes);
            }
            else
            { // sent_bytes == -1
                if (errno != EAGAIN && errno != EWOULDBLOCK)
                {
                    close_client(
                        client.get_fd(),
                        make_errno_message("send error").c_str()
                    );
                    return;
                }
            }
        }

        if (!client.get_write_buffer().empty() && !client.is_writing_registered())
        {
            auto mod_result
                = modify_descriptor(client.get_fd(), EPOLLIN | EPOLLOUT | EPOLLET);
            if (!mod_result.has_value())
            {
                close_client(client.get_fd(), "epoll_modify for EPOLLOUT failed");
                return;
            }
            client.is_writing_registered() = true;
        }
    }

    auto server::handle_client_read(client_connection &client) -> void
    {
        char buffer[BUFFER_SIZE];
        bool client_alive = true;

        while (true)
        {
            ssize_t bytes_received
                = recv(client.get_fd(), buffer, sizeof(buffer), 0);
            if (bytes_received > 0)
            {
                client.get_read_buffer().append(buffer, bytes_received);
            }
            else if (bytes_received == 0)
            {
                close_client(client.get_fd(), "client disconnected");
                client_alive = false;
                break;
            }
            else
            { // bytes_received == -1
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    break;
                close_client(
                    client.get_fd(),
                    make_errno_message("recv error").c_str()
                );
                client_alive = false;
                break;
            }
        }

        if (!client_alive)
            return;

        std::string &accumulated_data = client.get_read_buffer();
        size_t       newline_pos;
        while ((newline_pos = accumulated_data.find('\n')) != std::string::npos)
        {
            std::string command_line = accumulated_data.substr(0, newline_pos);
            accumulated_data.erase(0, newline_pos + 1);
            command_line = trim(command_line);

            if (!command_line.empty())
            {
                process_client_command(client, command_line);
            }
        }
    }

    auto server::handle_client_write(client_connection &client) -> void
    {
        if (client.get_write_buffer().empty())
        {
            if (client.is_writing_registered())
            {
                modify_descriptor(client.get_fd(), EPOLLIN | EPOLLET);
                client.is_writing_registered() = false;
            }
            return;
        }

        ssize_t bytes_sent = send(
            client.get_fd(),
            client.get_write_buffer().data(),
            client.get_write_buffer().length(),
            MSG_NOSIGNAL
        );

        if (bytes_sent >= 0)
        {
            client.get_write_buffer().erase(0, bytes_sent);
            if (client.get_write_buffer().empty()
                && client.is_writing_registered())
            {
                modify_descriptor(client.get_fd(), EPOLLIN | EPOLLET);
                client.is_writing_registered() = false;
            }
        }
        else
        { // bytes_sent == -1
            if (errno != EAGAIN && errno != EWOULDBLOCK)
            {
                close_client(
                    client.get_fd(),
                    make_errno_message("send error").c_str()
                );
            }
        }
    }

    auto server::make(uint16_t port, int backlog)
        -> std::expected<server, std::string>
    {
        // Step 1: Setup socket
        std::expected<scoped_file_descriptor, std::string> server_socket_fd_expected
            = [&]() -> std::expected<scoped_file_descriptor, std::string>
        {
            scoped_file_descriptor fd(socket(AF_INET, SOCK_STREAM, 0));
            if (fd.get() == -1)
                return std::unexpected(make_errno_message("socket() failed"));
            return fd;
        }();
        if (!server_socket_fd_expected)
        {
            return std::unexpected(server_socket_fd_expected.error());
        }
        scoped_file_descriptor server_socket_fd
            = std::move(server_socket_fd_expected.value());

        // Step 2: Configure socket
        auto configure_res = [&](scoped_file_descriptor fd)
            -> std::expected<scoped_file_descriptor, std::string>
        {
            int option_value = 1;
            if (setsockopt(
                    fd.get(),
                    SOL_SOCKET,
                    SO_REUSEADDR,
                    &option_value,
                    sizeof(option_value)
                )
                == -1)
            {
                return std::unexpected(make_errno_message("setsockopt(SO_"
                                                          "REUSEADDR) failed"));
            }
            if (auto result = make_socket_non_blocking(fd.get()); !result)
            { // Check has_value() implicitly
                return std::unexpected(result.error());
            }
            return fd;
        }(std::move(server_socket_fd));

        if (!configure_res)
        {
            return std::unexpected(configure_res.error());
        }
        server_socket_fd = std::move(configure_res.value());

        // Step 3: Bind and Listen
        auto bind_listen_res = [&](scoped_file_descriptor fd)
            -> std::expected<scoped_file_descriptor, std::string>
        {
            sockaddr_in server_address {};
            server_address.sin_family      = AF_INET;
            server_address.sin_addr.s_addr = INADDR_ANY;
            server_address.sin_port        = htons(port);
            if (bind(fd.get(), (struct sockaddr *)&server_address, sizeof(server_address))
                < 0)
            {
                return std::unexpected(make_errno_message("bind() failed"));
            }
            if (listen(fd.get(), backlog) < 0)
            {
                return std::unexpected(make_errno_message("listen() failed"));
            }
            return fd;
        }(std::move(server_socket_fd));

        if (!bind_listen_res)
        {
            return std::unexpected(bind_listen_res.error());
        }
        server_socket_fd = std::move(bind_listen_res.value());

        // Step 4: Create Epoll
        std::expected<scoped_file_descriptor, std::string> epoll_fd_expected =
            []() -> std::expected<scoped_file_descriptor, std::string>
        {
            scoped_file_descriptor fd(epoll_create1(0));
            if (fd.get() == -1)
                return std::unexpected(make_errno_message("epoll_create1 failed"));
            return fd;
        }();
        if (!epoll_fd_expected)
        {
            return std::unexpected(epoll_fd_expected.error());
        }
        scoped_file_descriptor epoll_fd = std::move(epoll_fd_expected.value());

        // Step 5: Construct server and register listening socket
        server new_server(std::move(epoll_fd), std::move(server_socket_fd));
        auto   register_res = new_server.register_descriptor(
            new_server.server_file_descriptor.get(),
            EPOLLIN | EPOLLET
        );
        if (!register_res)
        {
            return std::unexpected(register_res.error());
        }

        std::cout << "Server configured successfully on port " << port << "."
                  << std::endl;
        return new_server; // Implicit move
    }

    auto server::run(void) -> void
    {
        std::vector<epoll_event> events_vector(MAX_EPOLL_EVENTS);

        while (true)
        {
            int num_events = epoll_wait(
                epoll_file_descriptor.get(),
                events_vector.data(),
                MAX_EPOLL_EVENTS,
                -1
            );

            if (num_events == -1)
            {
                if (errno == EINTR)
                    continue;
                perror("epoll_wait error");
                break;
            }

            for (int i = 0; i < num_events; ++i)
            {
                int      current_fd       = events_vector[i].data.fd;
                uint32_t triggered_events = events_vector[i].events;

                if (current_fd == server_file_descriptor.get())
                {
                    if (triggered_events & EPOLLIN)
                    {
                        accept_new_connections();
                    }
                }
                else
                {
                    auto client_iterator = client_connections.find(current_fd);
                    if (client_iterator == client_connections.end())
                        continue;

                    client_connection &client = client_iterator->second;

                    if ((triggered_events & EPOLLERR)
                        || (triggered_events & EPOLLHUP))
                    {
                        close_client(current_fd, "EPOLLERR or EPOLLHUP");
                    }
                    else
                    {
                        if (triggered_events & EPOLLIN)
                        {
                            handle_client_read(client);
                        }
                        // Check if client still exists after read before
                        // attempting write
                        if (client_connections.count(current_fd)
                            && (triggered_events & EPOLLOUT))
                        {
                            handle_client_write(client);
                        }
                    }
                }
            }
        }
    }

    auto make_socket_non_blocking(int socket_fd)
        -> std::expected<void, std::string>
    {
        int flags = fcntl(socket_fd, F_GETFL, 0);
        if (flags == -1)
        {
            return std::unexpected(make_errno_message(
                "fcntl F_GETFL failed for fd " + std::to_string(socket_fd)
            ));
        }
        flags |= O_NONBLOCK;
        if (fcntl(socket_fd, F_SETFL, flags) == -1)
        {
            return std::unexpected(make_errno_message(
                "fcntl F_SETFL O_NONBLOCK failed for fd " + std::to_string(socket_fd)
            ));
        }
        return {};
    }

} // namespace oreore
