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
using Poco::Net::StreamSocket;
using Poco::Net::IPAddress;

void Networking::m_ping(CallbackBridge _callback) {
    Poco::Net::IPAddress server;
    try {
        server = m_resolve(g_laar_service);
    } catch (const Poco::Net::DNSException& _ex) {
        PLOG(plog::error) << "Ping to Laar server failed, host not found";
        m_state = NetState::ServerDown;
        _callback();
    } catch (const net::NoValidIPFoundException& _ex) {
        PLOG(plog::error) << "No valid IP address for pinging";
        m_state = NetState::ServerDown;
        _callback();
    }

    ICMPClient client {SocketAddress::Family::IPv4};
    auto replies = client.ping(server.toString());
    PLOG(plog::info) << "Replies for ping received: " << replies;
    if (!replies) {
        PLOG(plog::warning) << "No response from Laar server";
        m_state = NetState::ServerDown;
    }
    // Callback
    _callback();
}

IPAddress Networking::m_resolve(const std::string& _domain_name) {
    Poco::Net::HostEntry entries;
    try {
        entries = DNS::hostByName(_domain_name.data());
    } catch (const Poco::Net::DNSException& _ex) {
        PLOG(plog::error) << "Host resolution failed for host: "
                          << _domain_name;
        _ex.rethrow();
    }

    PLOG(plog::verbose) << "IP addresses acquired for " << _domain_name;

    auto addrs = entries.addresses();
    auto valid = std::find_if(addrs.begin(), addrs.end(),
        [](const IPAddress& addr) { return addr.family() == SocketAddress::IPv4; });

    if (valid == addrs.end())
        throw net::NoValidIPFoundException{_domain_name};
    PLOG(plog::verbose) << "Address chosen: " << valid->toString();
    return *valid;
}

void Networking::invoke_ping(const std::function<void(void)>* _callback) {
    if (m_ping_lock) {
        PLOG(plog::error) << "Ping is already running";
        throw net::LockViolation{};
    }
    m_ping_lock = true;

    net::CallbackBridge bridge {_callback, &m_ping_lock};
    m_ping_runnable = std::make_unique<tasks::PingTask>(this, bridge);
    Poco::Thread thread {};
    thread.start(*m_ping_runnable);
    thread.join();
}

void Networking::m_query_global_ip(CallbackBridge _callback) {
    URI uri{"http://" + g_glip_id_service};
    HTTPClientSession session {};
    session.setSourceAddress({m_host, 0});
    session.setHost(uri.getHost());
    session.setPort(uri.getPort());
    HTTPRequest req(HTTPRequest::HTTP_GET, "/", HTTPMessage::HTTP_1_1);
    session.sendRequest(req);
    HTTPResponse res {};
    auto& ifs = session.receiveResponse(res);
    // Response is a single IPv4 address.
    ifs >> m_global_host;
    PLOG(plog::info) << "Received global IP " << m_global_host;

    _callback();
}

void Networking::query_global_ip(const std::function<void(void)>* _callback) {
    if (m_query_global_ip_lock) {
        PLOG(plog::error) << "Query for global IP is already running";
        throw net::LockViolation{};
    }
    m_query_global_ip_lock = true;

    net::CallbackBridge bridge {_callback, &m_query_global_ip_lock};
    m_query_glip_runnable = std::make_unique<tasks::QueryGlobalIPTask>(this, bridge);
    Poco::Thread thread {};
    thread.start(*m_query_glip_runnable);
    thread.join();
}

void Networking::m_query_geolocation(CallbackBridge _callback) {
    if (!m_global_host.size()) {
        PLOG(plog::error) << "Host global IP must be acquired first";
        throw net::InvalidState{};
    }
    GeolocationQuery query{m_global_host};
    PLOG(plog::verbose) << "Sending query for geolocation: " << query.query();

    URI uri {query.query()};
    HTTPClientSession session {};
    session.setSourceAddress({m_host, 0});
    session.setHost(uri.getHost());
    session.setPort(uri.getPort());
    std::string path {uri.getPathAndQuery()};
    HTTPRequest req(HTTPRequest::HTTP_GET, path, HTTPMessage::HTTP_1_1);
    session.sendRequest(req);
    HTTPResponse res {};
    auto& ifs = session.receiveResponse(res);

    m_geolocation.clear();
    std::string next {};
    ifs >> next;
    if (next != "success") {
        PLOG(plog::error) << "API response was: " << next
                          << ", expected: success";
        throw net::NetException{"geolocation API returned failed"};
    }
    // Country
    if (!ifs) {
        m_geolocation = "Nowhere";
        PLOG(plog::warning) << "Response for location missing country, city and district";
        goto callback;
    }
    ifs >> next;
    m_geolocation += next;
    if (!ifs) {
        PLOG(plog::warning) << "Response for location missing city and district";
        goto callback;
    }
    ifs >> next;
    m_geolocation += ", " + next;
    if (!ifs) {
        PLOG(plog::warning) << "Response for location missing district";
        goto callback;
    }
    ifs >> next;
    m_geolocation += ", " + next;

    PLOG(plog::info) << "Received geolocation: " << m_geolocation;

    callback:
    _callback();
}

void Networking::query_geolocation(const std::function<void(void)>* _callback) {
    if (m_query_geolocation_lock) {
        PLOG(plog::error) << "Query for Geolocation is already running";
        throw net::LockViolation{};
    }
    m_query_geolocation_lock = true;

    net::CallbackBridge bridge {_callback, &m_query_geolocation_lock};
    m_query_location_runnable = std::make_unique<tasks::QueryGeolocationTask>(this, bridge);
    Poco::Thread thread {};
    thread.start(*m_query_location_runnable);
    thread.join();
}

Networking::Networking() {
    // Log initialization.
    plog::init(plog::verbose, "log.txt", c_mlogfsize, c_mlogfnum);
    // Get linked-list with hardware network interfaces.
    ifaddrs* m_ints;
    auto result = getifaddrs(&m_ints);
    if (result) {
        PLOG(plog::error) << "Acquiring network interfaces failed: "
                          << errno << " errno";
        throw net::SystemException(errno);
    }

    PLOG(plog::verbose) << "Parsing through hardware network interfaces";
    std::vector<std::pair<std::string, std::string>> active_ints;
    std::string host {};
    host.resize(NI_MAXHOST);

    for (auto curr = m_ints; curr; curr = curr->ifa_next) {
        auto family = curr->ifa_addr->sa_family;
        if (AF_INET == family) {
            auto res = getnameinfo(curr->ifa_addr, sizeof(sockaddr_in), host.data(),
                                   NI_MAXHOST, nullptr, 0, NI_NUMERICHOST);
            if (res) {
                PLOG(plog::error) << "Socket error: " << errno << " errno";
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

    freeifaddrs(m_ints);
}


