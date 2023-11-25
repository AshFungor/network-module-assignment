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

    class GeolocationQuery {
        constexpr static std::string c_service {"ip-api.com"};
        constexpr static std::string c_datatype {"line"};
        constexpr static std::string c_fields {"573457"};
        constexpr static std::string_view c_query {"http://{0}/{1}/{2}?fields={3}"};

        std::string m_query {};
    public:
        GeolocationQuery(const std::string& _ip) {
            m_query = std::vformat(c_query,
                std::make_format_args(c_service, c_datatype, _ip, c_fields));
        }

        std::string query() const { return m_query; }
    };

    class Networking {
        // Configuration
        constexpr static std::uint32_t c_mlogfsize = 1024 * 20;
        constexpr static std::uint32_t c_mlogfnum = 1;

        // Running instance
        static std::unique_ptr<Networking> __instance;
        std::atomic<NetState> m_state {};
        std::string m_host {};
        std::string m_global_host {};
        std::string m_geolocation {};

        void m_ping(const std::function<void(void)>* _callback);
        void m_query_global_ip(const std::function<void(void)>* _callback);
        void m_query_geolocation(const std::function<void(void)>* _callback);

        Poco::Net::IPAddress m_resolve(const std::string& _domain_name);

        Networking();
    public:
        void invoke_ping(const std::function<void(void)>* _callback = nullptr);
        void query_global_ip(const std::function<void(void)>* _callback = nullptr);
        void query_geolocation(const std::function<void(void)>* _callback = nullptr);
        NetState get_state() { return m_state; }
        std::string get_addr() { return m_host; }
        std::string get_global_addr() { return m_global_host; }
        std::string get_geolocation() { return m_geolocation; }

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
