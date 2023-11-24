#include "networking.hpp"

int main()
{
    auto& singleton = net::Networking::get_instance();
    // singleton.invoke_ping();
    singleton.query_global_ip();
    return 0;
}
