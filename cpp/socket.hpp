#pragma once
#include <string>
#include <string_view>
#include <cstdint>

namespace botbase {

class Socket {
public:
    Socket(const std::string& host, std::uint16_t port, const std::string& interface = {});
    ~Socket();
    void send_all(std::string_view data);
    std::size_t recv(char* buf, std::size_t n);
private:
    int fd_;
};

}
