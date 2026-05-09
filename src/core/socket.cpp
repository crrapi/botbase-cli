#include "socket.hpp"

#include <arpa/inet.h>
#include <fcntl.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <format>

#include "botbase/errors.hpp"
#include "botbase/signals.hpp"

#if defined(__APPLE__) && !defined(IP_BOUND_IF)
#define IP_BOUND_IF 25
#endif

#ifdef MSG_NOSIGNAL
constexpr int kSendFlags = MSG_NOSIGNAL;
#else
constexpr int kSendFlags = 0;
#endif

namespace botbase::detail {

namespace {

using Clock = std::chrono::steady_clock;

std::string system_error(std::string_view what, int error) {
    return std::format("{}: {}", what, std::strerror(error));
}

}  // namespace

Socket::~Socket() { close(); }

Socket::Socket(Socket&& other) noexcept : fd_(other.fd_) { other.fd_ = -1; }

Socket& Socket::operator=(Socket&& other) noexcept {
    if (this != &other) {
        close();
        fd_ = other.fd_;
        other.fd_ = -1;
    }
    return *this;
}

void Socket::close() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

Socket Socket::open_tcp(const std::string& interface) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) throw ConnectionError(system_error("socket", errno));
    Socket socket(fd);

    set_non_blocking(fd);
    int on = 1;
    ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof on);
#ifdef SO_NOSIGPIPE
    ::setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof on);
#endif
    if (!interface.empty()) bind_to_interface(fd, interface);
    return socket;
}

Socket Socket::connect(const std::string& host, std::uint16_t port, const std::string& interface,
                       double timeout) {
    sockaddr_in address = resolve_ipv4(host, port);
    Socket socket = open_tcp(interface);
    std::string target = std::format("{}:{}", host, port);

    if (::connect(socket.fd(), reinterpret_cast<sockaddr*>(&address), sizeof address) == 0) {
        return socket;
    }
    if (errno != EINPROGRESS) throw ConnectionError(system_error("cannot connect to " + target, errno));

    wait_for(socket.fd(), Wait::Writable, timeout, true, "timed out connecting to " + target);
    int error = 0;
    socklen_t length = sizeof error;
    ::getsockopt(socket.fd(), SOL_SOCKET, SO_ERROR, &error, &length);
    if (error != 0) throw ConnectionError(system_error("cannot connect to " + target, error));
    return socket;
}

void Socket::send_all(std::string_view data, double timeout) {
    while (!data.empty()) {
        ssize_t sent = ::send(fd_, data.data(), data.size(), kSendFlags);
        if (sent >= 0) {
            data.remove_prefix(static_cast<std::size_t>(sent));
        } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
            wait_for(fd_, Wait::Writable, timeout, false, "timed out sending to sys-botbase");
        } else if (errno != EINTR) {
            throw ConnectionError(system_error("connection lost", errno));
        }
    }
}

std::size_t Socket::receive(char* buffer, std::size_t size, double timeout) {
    for (;;) {
        ssize_t got = ::recv(fd_, buffer, size, 0);
        if (got >= 0) return static_cast<std::size_t>(got);
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            wait_for(fd_, Wait::Readable, timeout, true,
                     std::format("no reply from sys-botbase in {:g} s", timeout));
        } else if (errno != EINTR) {
            throw ConnectionError(system_error("connection lost", errno));
        }
    }
}

sockaddr_in resolve_ipv4(const std::string& host, std::uint16_t port) {
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    if (::inet_pton(AF_INET, host.c_str(), &address.sin_addr) == 1) return address;

    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* found = nullptr;
    int status = ::getaddrinfo(host.c_str(), nullptr, &hints, &found);
    if (status != 0 || found == nullptr) {
        throw ConnectionError(std::format("cannot resolve {}: {}", host, ::gai_strerror(status)));
    }
    address.sin_addr = reinterpret_cast<sockaddr_in*>(found->ai_addr)->sin_addr;
    ::freeaddrinfo(found);
    return address;
}

std::string format_ipv4(const sockaddr_in& address) {
    char text[INET_ADDRSTRLEN] = {};
    ::inet_ntop(AF_INET, &address.sin_addr, text, sizeof text);
    return text;
}

void bind_to_interface(int fd, const std::string& interface) {
#if defined(__APPLE__)
    unsigned index = ::if_nametoindex(interface.c_str());
    if (index == 0) throw ConnectionError("no network interface named " + interface);
    if (::setsockopt(fd, IPPROTO_IP, IP_BOUND_IF, &index, sizeof index) != 0) {
        throw ConnectionError(system_error("cannot bind to " + interface, errno));
    }
#elif defined(__linux__)
    if (::setsockopt(fd, SOL_SOCKET, SO_BINDTODEVICE, interface.c_str(),
                     static_cast<socklen_t>(interface.size() + 1)) != 0) {
        throw ConnectionError(system_error("cannot bind to " + interface, errno));
    }
#else
    (void)fd;
    throw ConnectionError("binding to an interface is not supported on this platform");
#endif
}

void set_non_blocking(int fd) {
    int flags = ::fcntl(fd, F_GETFL, 0);
    ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

void wait_for(int fd, Wait what, double timeout, bool interruptible, const std::string& timeout_message) {
    using std::chrono::duration;
    using std::chrono::milliseconds;

    const auto deadline = Clock::now() + std::chrono::duration_cast<Clock::duration>(duration<double>(timeout));
    pollfd entry{fd, static_cast<short>(what == Wait::Readable ? POLLIN : POLLOUT), 0};

    for (;;) {
        if (interruptible) throw_if_interrupted();
        auto left = std::chrono::ceil<milliseconds>(deadline - Clock::now()).count();
        if (left <= 0) throw TimeoutError(timeout_message);

        // Short slices so a Ctrl-C is noticed quickly.
        int ready = ::poll(&entry, 1, static_cast<int>(std::min<long long>(left, 50)));
        if (ready > 0) return;
        if (ready < 0 && errno != EINTR) throw ConnectionError(system_error("poll", errno));
    }
}

}  // namespace botbase::detail
