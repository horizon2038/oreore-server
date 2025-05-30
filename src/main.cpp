#include <oreore/message.hpp>
#include <oreore/server.hpp>

#include <cstdlib>
#include <iostream>

inline constexpr const char logo[] = R"(
  ___  _ __ ___  ___  _ __ ___       ___  ___ _ ____   _____ _ __
 / _ \| '__/ _ \/ _ \| '__/ _ \_____/ __|/ _ \ '__\ \ / / _ \ '__|
| (_) | | |  __/ (_) | | |  __/_____\__ \  __/ |   \ V /  __/ |
 \___/|_|  \___|\___/|_|  \___|     |___/\___|_|    \_/ \___|_|

)";

auto main(int argc, const char *argv[]) -> int
{
    // get the port from the command line arguments, if provided
    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " <port>" << std::endl;
        return EXIT_FAILURE;
    }
    uint16_t port = std::atoi(argv[1]) & 0xFFFF; // 16-bit port number

    std::cout << logo << std::endl;

    auto server_expected = oreore::server::make(port, oreore::BACKLOG_SIZE);

    if (!server_expected.has_value())
    {
        std::cerr << "FATAL: Failed to initialize server: "
                  << server_expected.error() << std::endl;
        return EXIT_FAILURE;
    }

    oreore::server my_server = std::move(server_expected.value());
    my_server.run();

    std::cout << "Application terminating." << std::endl;
    return EXIT_SUCCESS;
}
