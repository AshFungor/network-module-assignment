#include <iostream>
#include <Poco/Thread.h>
#include <Poco/Runnable.h>

#include "networking.hpp"

struct IdentTask : public Poco::Runnable {
    virtual void run() override {
        auto& singleton = net::Networking::get_instance();
        std::function<void(void)> callback_for_ping = [&]() {
            auto state = singleton.get_state();
            std::cout << "Local state: "
                      << ((state == net::NetState::IntDown) ? "Interfaces down" :
                      (state == net::NetState::ServerDown)  ? "Server down" :
                      (state == net::NetState::Eather)      ? "Ethernet connection" :
                                                              "Wireless connection") << '\n';
            std::cout << "Local address: " << singleton.get_addr() << '\n';
        };
        singleton.invoke_ping(&callback_for_ping);

        std::function<void(void)> callback_for_gl_addr = [&]()
            { std::cout << "Global address: " << singleton.get_global_addr() << '\n'; };

        std::function<void(void)> callback_for_geoloc = [&]()
            { std::cout << "Geolocation: " << singleton.get_geolocation() << '\n'; };

        singleton.query_global_ip(&callback_for_gl_addr);
        singleton.query_geolocation(&callback_for_geoloc);
    }
};

int main()
{
    Poco::Thread net_thread {};
    IdentTask task {};
    net_thread.start(task);
    net_thread.join();

    // Do other stuff

    return 0;
}
