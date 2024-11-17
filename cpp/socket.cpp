#include "socket.hpp"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#include <stdexcept>
#include <cstring>

#if defined(__APPLE__) && !defined(IP_BOUND_IF)
#define IP_BOUND_IF 25
#endif

namespace botbase {

Socket::Socket(const std::string& host, std::uint16_t port, const std::string&) {
    fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port = htons(port);
    ::inet_pton(AF_INET, host.c_str(), &a.sin_addr);
    if (::connect(fd_, (sockaddr*)&a, sizeof a) < 0) throw std::runtime_error("connect");
}

Socket::~Socket() { if (fd_ >= 0) ::close(fd_); }

void Socket::send_all(std::string_view data) {
    ::send(fd_, data.data(), data.size(), 0);
}

std::size_t Socket::recv(char* buf, std::size_t n) {
    ssize_t r = ::recv(fd_, buf, n, 0);
    return r > 0 ? r : 0;
}

}
