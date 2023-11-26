// STD
#include <format>
#include <exception>
#include <string_view>
#include <string>

// GLIBC
#include <errno.h>
#include <netdb.h>

#pragma once

namespace net {

    class NetException : public std::exception {
        std::string m_error_msg {};
    public:
        NetException(std::string _error_msg)
        : m_error_msg{_error_msg} {}

        const char* what() const noexcept override {
            return m_error_msg.data();
        }
    };

    class SystemException : public NetException {
        static constexpr std::string_view c_fmt_str =
                         "System call returned {} errno";
    public:
        SystemException(int _error)
            : NetException{std::format(c_fmt_str, _error)}
        {}
    };

    class SocketException : public NetException {
        static constexpr std::string_view c_fmt_str =
                         "Socket returned error {}";
    public:
        SocketException(int _error)
            : NetException{std::format(c_fmt_str, _error)}
        {}
    };

    class NoValidIPFoundException : public NetException {
        static constexpr std::string_view c_fmt_str =
                         "Failed to acquire IPv4 address for server {}";
    public:
        NoValidIPFoundException(std::string_view _host)
            : NetException{std::format(c_fmt_str, _host)}
        {}
    };

    class InvalidState : public NetException {
        static constexpr std::string_view c_nofmt_str =
                         "State of singleton is invalid (was queries invoked in correct order?)";
    public:
        InvalidState()
            : NetException{c_nofmt_str.data()}
        {}
    };

    class LockViolation : public NetException {
        static constexpr std::string_view c_nofmt_str =
                         "Lock violation: network call is already running";
    public:
        LockViolation()
            : NetException{c_nofmt_str.data()}
        {}
    };

}
