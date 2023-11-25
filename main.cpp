#include <iostream>

#include "networking.hpp"

int main()
{
    auto& singleton = net::Networking::get_instance();
    singleton.invoke_ping();
    auto state = singleton.get_state();
    std::cout << "Local state: "
              << ((state == net::NetState::IntDown) ? "Interfaces down" :
              (state == net::NetState::ServerDown)  ? "Server down" :
              (state == net::NetState::Eather)      ? "Ethernet connection" :
                                                      "Wireless connection") << '\n';
    std::cout << "Local address: " << singleton.get_addr() << '\n';

    std::function<void(void)> callback_for_gl_addr = [&]()
        { std::cout << "Global address: " << singleton.get_global_addr() << '\n'; };

    std::function<void(void)> callback_for_geoloc = [&]()
        { std::cout << "Geolocation: " << singleton.get_geolocation() << '\n'; };

    singleton.query_global_ip(&callback_for_gl_addr);
    singleton.query_geolocation(&callback_for_geoloc);

    return 0;
}
