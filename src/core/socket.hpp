#pragma once

#include <netinet/in.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace botbase::detail {

// A non-blocking TCP socket whose waits honour timeouts and Ctrl-C.
class Socket {
public:
    Socket() = default;
    explicit Socket(int fd) : fd_(fd) {}
    ~Socket();

    Socket(Socket&& other) noexcept;
    Socket& operator=(Socket&& other) noexcept;
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    static Socket open_tcp(const std::string& interface);
    static Socket connect(const std::string& host, std::uint16_t port,
                          const std::string& interface, double timeout);

    int fd() const { return fd_; }
    void close();

    // Sends are never cut short by Ctrl-C: half a command line would merge
    // with the next one on the console.
    void send_all(std::string_view data, double timeout);
    // Returns 0 when the peer closed the connection.
    std::size_t receive(char* buffer, std::size_t size, double timeout);

private:
    int fd_ = -1;
};

sockaddr_in resolve_ipv4(const std::string& host, std::uint16_t port);
std::string format_ipv4(const sockaddr_in& address);
void bind_to_interface(int fd, const std::string& interface);
void set_non_blocking(int fd);

enum class Wait { Readable, Writable };
void wait_for(int fd, Wait what, double timeout, bool interruptible, const std::string& timeout_message);

}  // namespace botbase::detail
