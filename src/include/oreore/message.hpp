#ifndef OREORE_MESSAGE_HPP
#define OREORE_MESSAGE_HPP

#include <stdint.h>
#include <string>

namespace oreore
{
    inline constexpr int BACKLOG_SIZE     = 128; // listen backlog often int
    inline constexpr int MAX_EPOLL_EVENTS = 64;  // Max events for epoll_wait
    inline constexpr size_t BUFFER_SIZE = 4096; // For individual read operations

    struct message
    {
        uintmax_t   id;
        std::string text;
        std::string sender_ip;
        std::string reaction;
    };

    auto make_errno_message(const std::string &base_message) -> std::string;
    auto trim(const std::string &str) -> std::string;

}

#endif
