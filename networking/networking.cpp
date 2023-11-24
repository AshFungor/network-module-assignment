// Local
#include "networking.hpp"
#include "net_exceptions.hpp"

using namespace net;

std::unique_ptr<Networking> Networking::__instance = nullptr;

using Poco::URI;
using Poco::Net::DNS;
using Poco::Net::ICMPClient;
using Poco::Net::SocketAddress;
using Poco::Net::HTTPClientSession;
using Poco::Net::HTTPRequest;
using Poco::Net::HTTPResponse;
using Poco::Net::HTTPMessage;
using Poco::Net::ICMPSocketImpl;
using Poco::Net::IPAddress;

void Networking::m_ping(const std::function<void(void)>* _callback) {
    auto dest_addr = m_resolve(g_laar_domain_name).toString();
    ICMPClient client {SocketAddress::Family::IPv4};
    auto replies = client.pingIPv4(dest_addr);
    PLOG(plog::info) << "Ping result is " << (replies > 0);
    if (!replies) m_state = NetState::ServerDown;
    // Callback
    if (!_callback) return;
    (*_callback)();
}

IPAddress Networking::m_resolve(const std::string& _domain_name) {
    auto entries = DNS::hostByName(_domain_name.data());
    PLOG(plog::info) << "Getting entries for " << _domain_name;
    auto addrs = entries.addresses();
    auto valid = std::find_if(addrs.begin(), addrs.end(),
        [](IPAddress& addr) { return addr.family() == SocketAddress::IPv4; });
    if (valid == addrs.end()) throw net::DNSException{};
    PLOG(plog::info) << "Address resolved: " << valid->toString();
    return *valid;
}

void Networking::invoke_ping(const std::function<void(void)>* _callback) {
    std::thread _ping_job {&Networking::m_ping, this, _callback};
    _ping_job.join();
}

void Networking::m_query_global_ip(const std::function<void(void)>* _callback) {
    auto dest_addr = m_resolve(g_global_ip_id_domain_name);
    URI uri{"http://" + g_global_ip_id_domain_name};
    auto session_sock = m_httpsock_factory->create();
    HTTPClientSession session {uri.getHost(), uri.getPort()};
    std::string path {uri.getPathAndQuery()};
    if (path.empty()) path = "/";
    HTTPRequest req(HTTPRequest::HTTP_GET, path, HTTPMessage::HTTP_1_1);
    session.sendRequest(req);
    HTTPResponse res {};
    auto& ifs = session.receiveResponse(res);
    std::string res_ip {};
    ifs >> res_ip;
    PLOG(plog::info) << "Received global IP " << res_ip;

}

void Networking::query_global_ip(const std::function<void(void)>* _callback) {
    std::thread _query_job {&Networking::m_query_global_ip, this, _callback};
    _query_job.join();
}

Networking::Networking() {
    // Log initialization.
    plog::init(plog::info, "log.txt", c_mlogfsize, c_mlogfnum);
    // Get linked-list with hardware network interfaces.
    ifaddrs* m_ints;
    auto result = getifaddrs(&m_ints);
    if (result) {
        throw net::SystemException(errno);
    }

    PLOG(plog::info) << "Parsing through hardware network interfaces";
    std::vector<std::pair<std::string, std::string>> active_ints;
    std::string host {};
    host.resize(NI_MAXHOST);

    for (auto curr = m_ints; curr; curr = curr->ifa_next) {
        auto family = curr->ifa_addr->sa_family;
        if (AF_INET == family) {
            auto res = getnameinfo(curr->ifa_addr,
                                   sizeof(sockaddr_in),
                                   host.data(),
                                   NI_MAXHOST,
                                   nullptr,
                                   0,
                                   NI_NUMERICHOST);
            if (res) {
                throw net::SocketException{res};
            }
        }
        PLOG(plog::info) << "Interface " << curr->ifa_name << " with address "
                         << ((AF_INET == family) ? host : "NONE");
        PLOG(plog::info) << "IP family: " << ((AF_INET == family) ? "IPv4" :
                                             (AF_INET6 == family) ? "IPv6" :
                                             "Unix socket (Pipeline)");
        PLOG(plog::info) << "Flags: "
                         << "State: " << (bool)(curr->ifa_flags & IFF_RUNNING) << ' '
                         << "UP: "    << (bool)(curr->ifa_flags & IFF_UP) << ' '
                         << "LO "     << (bool)(curr->ifa_flags & IFF_LOOPBACK);
        if (curr->ifa_flags & IFF_RUNNING   &&
            !(curr->ifa_flags & IFF_LOOPBACK) &&
            (family == AF_INET))
        {
            PLOG(plog::info) << "Adding to active pool: " << curr->ifa_name;
            active_ints.push_back({host, curr->ifa_name});
        }
    }

    if (active_ints.empty()) {
        m_state = NetState::IntDown;
    } else {
        for (auto& interface_data : active_ints) {
            std::string& name = interface_data.second;
            if (name.starts_with("eth") || name.starts_with("enp")) {
                m_host = interface_data.first;
                m_state = NetState::Eather;
            } else {
                // Escape if host is already specified.
                if (m_host.size()) continue;
                m_host = interface_data.first;
                m_state = NetState::Wireless;
            }
        }
    }

    m_icmpsock_factory = std::make_unique<ICMPSocketFactory>(m_host);
    m_httpsock_factory = std::make_unique<HTTPSocketFactory>(m_host);

    freeifaddrs(m_ints);
}


