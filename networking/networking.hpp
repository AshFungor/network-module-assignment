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
#include <Poco/Runnable.h>
#include <Poco/Net/DNS.h>
#include <Poco/URI.h>
#include <Poco/Net/NetException.h>
#include <Poco/Net/HTTPClientSession.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/Net/StreamSocket.h>
#include <Poco/Net/StreamSocketImpl.h>
#include "Poco/ThreadPool.h"

// Logging
#include <plog/Log.h>
#include <plog/Initializers/RollingFileInitializer.h>
#include <plog/Severity.h>

#pragma once

namespace net {

    constexpr std::string g_laar_service {"google.com"};
    constexpr std::string g_glip_id_service {"ident.me"};
    constexpr std::string g_geolocation_service {"ip-api.com"};

    enum class NetState : std::uint8_t {
        IntDown, Eather, Wireless, ServerDown
    };

    class GeolocationQuery {
        constexpr static std::string c_service {g_geolocation_service};
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

    using callback_func_t = const std::function<void(void)>*;

    class Networking {
        using netcall_mapped_t = void (Networking::* const) (callback_func_t);
        // Configuration
        constexpr static std::uint32_t c_mlogfsize = 1024 * 20;
        constexpr static std::uint32_t c_mlogfnum = 1;

        // Running instance
        static std::unique_ptr<Networking> __instance;

        // Locals
        std::atomic<NetState> m_state {};
        std::string m_host {};
        std::string m_global_host {};
        std::string m_geolocation {};

        // Locks
        bool m_ping_lock {false};
        bool m_query_global_ip_lock {false};
        bool m_query_geolocation_lock {false};

        void m_ping(callback_func_t _callback);
        void m_query_global_ip(callback_func_t _callback);
        void m_query_geolocation(callback_func_t _callback);

        Poco::Net::IPAddress m_resolve(const std::string& _domain_name);

        Networking();
    public:
        void invoke_ping(callback_func_t _callback = nullptr);
        void query_global_ip(callback_func_t _callback = nullptr);
        void query_geolocation(callback_func_t _callback = nullptr);
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

    namespace tasks {

        class NetTask : public Poco::Runnable {
        protected:
            Networking* m_master_ptr;
            callback_func_t m_callback_func;
        public:
            NetTask(Networking& _module, callback_func_t _callback)
            : m_master_ptr{&_module}, m_callback_func{_callback}
            {}
            NetTask() = delete;
        };

        class PingTask : public NetTask {
        public:
            PingTask(Networking& _module, callback_func_t _callback)
            : NetTask{_module, _callback} {}
            virtual void run() final {
                m_master_ptr->invoke_ping(m_callback_func);
            }
        };

        class QueryGlobalIPTask : public NetTask {
        public:
            QueryGlobalIPTask(Networking& _module, callback_func_t _callback)
            : NetTask{_module, _callback} {}
            virtual void run() final {
                m_master_ptr->query_global_ip(m_callback_func);
            }
        };

        class QueryGeolocationTask : public NetTask {
        public:
            QueryGeolocationTask(Networking& _module, callback_func_t _callback)
            : NetTask{_module, _callback} {}
            virtual void run() final {
                m_master_ptr->query_geolocation(m_callback_func);
            }
        };
    }

}
