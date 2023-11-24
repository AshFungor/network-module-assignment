// STD
#include <cstdint>
#include <stdexcept>
#include <format>
#include <vector>
#include <utility>
#include <thread>
#include <atomic>
#include <functional>
#include <memory>
#include <cmath>
#include <algorithm>

// GLIBC
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <ifaddrs.h>

// Poco
#include <Poco/Net/ICMPClient.h>
#include <Poco/Net/SocketAddress.h>
#include <Poco/Net/ICMPSocketImpl.h>
#include <Poco/Net/DNS.h>
#include <Poco/URI.h>
#include <Poco/Net/HTTPClientSession.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/Net/StreamSocket.h>
#include <Poco/Net/StreamSocketImpl.h>

// Logging
#include <plog/Log.h>
#include <plog/Initializers/RollingFileInitializer.h>
#include <plog/Severity.h>

#pragma once

namespace net {

    constexpr std::string g_laar_domain_name {"google.com"};
    constexpr std::string g_global_ip_id_domain_name {"ident.me"};

    enum class NetState : std::uint8_t {
        IntDown, Eather, Wireless, ServerDown
    };

    class BaseSocketFactory {
        // Port pool
    protected:
        constexpr static int c_low = 2000;
        constexpr static int c_high = 6000;
    public:
        BaseSocketFactory() = default;
        virtual std::unique_ptr<Poco::Net::Socket> create() = 0;
    };

    class ICMPSocketFactory : public BaseSocketFactory {
        // Configuration.
        constexpr static std::uint16_t c_port = 2000;
        constexpr static std::uint16_t c_pkgsize = 48;
        constexpr static std::uint16_t c_ttl = 128;
        constexpr static std::uint32_t c_timeout = 5000000;

        Poco::Net::SocketAddress m_sock_addr {};
    public:
        ICMPSocketFactory(std::string _local_addr, std::uint16_t _port = c_port)
        : BaseSocketFactory{}, m_sock_addr {_local_addr, _port} {}
        virtual std::unique_ptr<Poco::Net::Socket> create() final;
    };

    class HTTPSocketFactory : public BaseSocketFactory {
        // Configuration.
        constexpr static std::uint16_t c_port = 2000;

        Poco::Net::SocketAddress m_sock_addr {};
    public:
        HTTPSocketFactory(std::string _local_addr, std::uint16_t _port = c_port)
        : BaseSocketFactory{}, m_sock_addr {_local_addr, _port} {}
        virtual std::unique_ptr<Poco::Net::Socket> create() final;

    };

    class Networking {
        // Configuration
        constexpr static std::uint32_t c_mlogfsize = 1024 * 20;
        constexpr static std::uint32_t c_mlogfnum = 1;

        // Running instance
        static std::unique_ptr<Networking> __instance;
        std::atomic<NetState> m_state {};
        std::string m_host {};
        std::unique_ptr<ICMPSocketFactory> m_icmpsock_factory {};
        std::unique_ptr<HTTPSocketFactory> m_httpsock_factory {};

        void m_ping(const std::function<void(void)>* _callback);
        void m_query_global_ip(const std::function<void(void)>* _callback);

        Poco::Net::IPAddress m_resolve(const std::string& _domain_name);

        Networking();
    public:
        void invoke_ping(const std::function<void(void)>* _callback = nullptr);
        void query_global_ip(const std::function<void(void)>* _callback = nullptr);
        NetState get_state() { return m_state; }

        // Singleton
        Networking(const Networking&) = delete;
        Networking(Networking&&) = delete;
        static Networking& get_instance() {
            if (__instance) return *__instance;
            __instance.reset(new Networking{});
            return *__instance;
        }

    };

}
