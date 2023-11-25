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
                         "system call returned {} errno";
    public:
        SystemException(int _error)
            : NetException{std::vformat(c_fmt_str,
                      std::make_format_args(_error))}
        {}
    };

    class SocketException : public NetException {
        static constexpr std::string_view c_fmt_str =
                         "socket returned error {}";
    public:
        SocketException(int _error)
            : NetException{std::vformat(c_fmt_str,
                      std::make_format_args(gai_strerror(_error)))}
        {}
    };

    class DNSException : public NetException {
        static constexpr std::string_view c_nofmt_str =
                         "DNS resolution unsuccessful";
    public:
        DNSException()
            : NetException{c_nofmt_str.data()}
        {}
    };

    class InvalidState : public NetException {
        static constexpr std::string_view c_nofmt_str =
                         "state of singleton is invalid (was queries invoked in correct order?)";
    public:
        InvalidState()
            : NetException{c_nofmt_str.data()}
        {}
    };

}
