#include <oreore/message.hpp>

#include <cerrno>
#include <cstring>

namespace oreore
{
    auto make_errno_message(const std::string &base_message) -> std::string
    {
        return base_message + ": " + strerror(errno);
    }

    auto trim(const std::string &str) -> std::string
    {
        size_t first = str.find_first_not_of(" \t\n\r\f\v");
        if (first == std::string::npos)
        {
            return ""; // String is all whitespace
        }

        size_t last = str.find_last_not_of(" \t\n\r\f\v");

        return str.substr(first, last - first + 1);
    }
}
