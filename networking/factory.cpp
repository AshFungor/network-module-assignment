#include "networking.hpp"

using namespace net;

using Poco::Net::SocketAddress;
using Poco::Net::ICMPSocketImpl;
using Poco::Net::ICMPSocket;
using Poco::Net::StreamSocket;
using Poco::Net::Socket;

std::unique_ptr<Socket> ICMPSocketFactory::create() {
    auto icmp_sock_ptr = std::make_unique<ICMPSocket>(SocketAddress::Family::IPv4,
                                                     c_pkgsize,
                                                     c_ttl,
                                                     c_timeout);
    icmp_sock_ptr->impl()->bind(m_sock_addr);
    m_sock_addr = {m_sock_addr.host(),
        (std::uint16_t) std::clamp(m_sock_addr.port() + 1, c_low, c_high)};
    return std::move(icmp_sock_ptr);
}

std::unique_ptr<Poco::Net::Socket> HTTPSocketFactory::create() {
    auto http_sock_ptr = std::make_unique<StreamSocket>();
    PLOG(plog::info) << "Binding new HTTP socket to " << m_sock_addr.toString();
    http_sock_ptr->bind(m_sock_addr);
    m_sock_addr = {m_sock_addr.host(),
        (std::uint16_t) std::clamp(m_sock_addr.port() + 1, c_low, c_high)};
    return std::move(http_sock_ptr);
}
