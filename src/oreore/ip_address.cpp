#include <oreore/ip_address.hpp>

namespace oreore
{

    ip_address::ip_address(const std::string &addr_str)
        : optional_address_string(addr_str)
    {
        auto convert_to_raw =
            [](const std::string &input_address_string) -> std::optional<uint32_t>
        {
            in_addr addr_in;
            if (inet_pton(AF_INET, input_address_string.c_str(), &addr_in) == 1)
            {
                return ntohl(addr_in.s_addr);
            }

            return std::nullopt;
        };

        if (optional_address_string)
        {
            optional_address_raw = convert_to_raw(*optional_address_string);
        }
    }

    ip_address::ip_address(uint32_t addr_raw) : optional_address_raw(addr_raw)
    {
        auto convert_to_string =
            [](uint32_t input_address_raw) -> std::optional<std::string>
        {
            in_addr addr_in;
            addr_in.s_addr = htonl(input_address_raw);
            char buffer[INET_ADDRSTRLEN];
            if (inet_ntop(AF_INET, &addr_in, buffer, sizeof(buffer)) != nullptr)
            {
                return std::string(buffer);
            }

            return std::nullopt;
        };

        if (optional_address_raw)
        {
            optional_address_string = convert_to_string(*optional_address_raw);
        }
    }

    ip_address::ip_address(ip_address &&other) noexcept
        : optional_address_string(std::move(other.optional_address_string))
        , optional_address_raw(std::move(other.optional_address_raw))
    {
        other.optional_address_string.reset();
        other.optional_address_raw.reset();
    }

    auto ip_address::operator=(ip_address &&other) noexcept -> ip_address &
    {
        if (this != &other)
        {
            optional_address_string = std::move(other.optional_address_string);
            optional_address_raw    = std::move(other.optional_address_raw);
            other.optional_address_string.reset();
            other.optional_address_raw.reset();
        }

        return *this;
    }

    auto ip_address::make(const std::string &address_str)
        -> std::expected<ip_address, std::string>
    {
        if (address_str.empty())
        {
            return std::unexpected("ip_address::make error: Empty address "
                                   "string provided.");
        }
        ip_address instance(address_str);
        if (!instance.optional_address_raw.has_value()
            || !instance.optional_address_string.has_value())
        {
            return std::unexpected(
                "ip_address::make error: Invalid IP string or conversion "
                "failed: "
                + address_str
            );
        }

        return instance;
    }

    auto ip_address::make(uint32_t address_value)
        -> std::expected<ip_address, std::string>
    {
        ip_address instance(address_value);
        if (!instance.optional_address_string.has_value()
            || !instance.optional_address_raw.has_value())
        {
            return std::unexpected(
                "ip_address::make error: Failed to convert raw IP or invalid "
                "value: "
                + std::to_string(address_value)
            );
        }
        return instance;
    }

    auto ip_address::get_string(void) const -> const std::optional<std::string> &
    {
        return optional_address_string;
    }

    auto ip_address::get_raw(void) const -> const std::optional<uint32_t> &
    {
        return optional_address_raw;
    }

}
