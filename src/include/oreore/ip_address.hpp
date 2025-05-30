#ifndef OREORE_IP_ADDRESS_HPP
#define OREORE_IP_ADDRESS_HPP

#include <oreore/message.hpp>

#include <arpa/inet.h>
#include <expected>
#include <netinet/in.h>
#include <optional>
#include <sys/socket.h>

namespace oreore
{

    class ip_address
    {
      private:
        std::optional<std::string> optional_address_string;
        std::optional<uint32_t>    optional_address_raw;

        ip_address(const std::string &addr_str);
        ip_address(uint32_t addr_raw);

      public:
        ip_address(void)                                   = delete;
        ip_address(const ip_address &)                     = default;
        auto operator=(const ip_address &) -> ip_address & = default;
        ip_address(ip_address &&other) noexcept;
        auto operator=(ip_address &&other) noexcept -> ip_address &;

        static auto make(const std::string &address_str)
            -> std::expected<ip_address, std::string>;
        static auto make(uint32_t address_val)
            -> std::expected<ip_address, std::string>;

        [[nodiscard]] auto get_string(void) const
            -> const std::optional<std::string> &;
        [[nodiscard]] auto get_raw(void) const -> const std::optional<uint32_t> &;
    };

} // namespace oreore

#endif // OREORE_IP_ADDRESS_HPP
