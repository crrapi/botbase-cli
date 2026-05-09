#include "botbase/discovery.hpp"

#include <arpa/inet.h>
#include <poll.h>
#include <sys/socket.h>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <format>

#include "botbase/errors.hpp"
#include "botbase/signals.hpp"
#include "botbase/util.hpp"
#include "socket.hpp"

namespace botbase {

namespace {

using Clock = std::chrono::steady_clock;
using detail::Socket;

constexpr std::size_t kMaxHosts = 4096;
constexpr std::size_t kMaxInFlight = 128;  // stays under macOS's default limit of 256 open files
constexpr double kReplyTimeout = 1.5;      // a reply waits for one main-loop tick on the console

Clock::time_point after(double seconds) {
    return Clock::now() + std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(seconds));
}

std::vector<std::uint32_t> subnet_hosts(const std::string& subnet) {
    auto slash = subnet.find('/');
    std::string address = subnet.substr(0, slash);
    int prefix = slash == std::string::npos ? 32 : parse_int(subnet.substr(slash + 1));
    if (prefix < 0 || prefix > 32) throw UsageError(std::format("bad subnet '{}'", subnet));

    in_addr parsed{};
    if (::inet_pton(AF_INET, address.c_str(), &parsed) != 1) throw UsageError(std::format("bad subnet '{}'", subnet));

    std::uint64_t count = std::uint64_t{1} << (32 - prefix);
    if (count > kMaxHosts) throw UsageError(std::format("{} is too large to scan; use a /20 or smaller", subnet));

    std::uint32_t mask = prefix == 0 ? 0 : ~std::uint32_t{0} << (32 - prefix);
    std::uint32_t network = ntohl(parsed.s_addr) & mask;

    std::vector<std::uint32_t> hosts;
    if (prefix >= 31) {
        for (std::uint64_t i = 0; i < count; ++i) hosts.push_back(network + static_cast<std::uint32_t>(i));
    } else {
        for (std::uint64_t i = 1; i + 1 < count; ++i) hosts.push_back(network + static_cast<std::uint32_t>(i));
    }
    return hosts;
}

std::string ipv4_text(std::uint32_t host) {
    in_addr address{htonl(host)};
    char text[INET_ADDRSTRLEN] = {};
    ::inet_ntop(AF_INET, &address, text, sizeof text);
    return text;
}

std::optional<std::string> version_from(const std::string& data) {
    auto line = trim(std::string_view(data).substr(0, data.find('\n')));
    if (data.find('\n') == std::string::npos || !is_version_string(line)) return std::nullopt;
    return std::string(line);
}

struct Probe {
    std::uint32_t host;
    Socket socket;
    bool connected = false;
    Clock::time_point deadline;
    std::string received{};
    bool finished = false;
};

}  // namespace

std::string local_ipv4(const std::string& interface) {
    int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) throw ConnectionError("cannot open a UDP socket");
    Socket socket(fd);
    if (!interface.empty()) detail::bind_to_interface(fd, interface);

    // Connecting a UDP socket picks a route without sending anything.
    sockaddr_in target = detail::resolve_ipv4("192.0.2.1", 9);
    if (::connect(fd, reinterpret_cast<sockaddr*>(&target), sizeof target) != 0) {
        throw ConnectionError("no route to the local network" + (interface.empty() ? "" : " on " + interface));
    }
    sockaddr_in local{};
    socklen_t length = sizeof local;
    ::getsockname(fd, reinterpret_cast<sockaddr*>(&local), &length);
    return detail::format_ipv4(local);
}

std::string default_subnet(const std::string& interface) {
    std::string ip = local_ipv4(interface);
    return ip.substr(0, ip.rfind('.')) + ".0/24";
}

std::optional<std::string> probe(const std::string& host, std::uint16_t port, const std::string& interface,
                                 double timeout) {
    try {
        Socket socket = Socket::connect(host, port, interface, timeout);
        socket.send_all("getVersion\r\n", timeout);
        std::string data;
        char chunk[64];
        while (data.find('\n') == std::string::npos && data.size() < 64) {
            std::size_t got = socket.receive(chunk, sizeof chunk, std::max(timeout, kReplyTimeout));
            if (got == 0) break;
            data.append(chunk, got);
        }
        return version_from(data);
    } catch (const Interrupted&) {
        throw;
    } catch (const Error&) {
        return std::nullopt;
    }
}

std::vector<Console> discover(const std::string& subnet, std::uint16_t port, const std::string& interface,
                              double timeout) {
    const std::vector<std::uint32_t> hosts = subnet_hosts(subnet);
    std::vector<std::pair<std::uint32_t, std::string>> found;
    std::vector<Probe> active;
    std::size_t next = 0;

    auto start = [&](std::uint32_t host) {
        Probe probe{.host = host, .socket = Socket::open_tcp(interface), .deadline = after(timeout)};
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(port);
        address.sin_addr.s_addr = htonl(host);
        int result = ::connect(probe.socket.fd(), reinterpret_cast<sockaddr*>(&address), sizeof address);
        if (result != 0 && errno != EINPROGRESS) return;
        active.push_back(std::move(probe));
    };

    while (next < hosts.size() || !active.empty()) {
        throw_if_interrupted();
        while (active.size() < kMaxInFlight && next < hosts.size()) start(hosts[next++]);

        std::vector<pollfd> fds;
        for (const Probe& p : active) fds.push_back({p.socket.fd(), static_cast<short>(p.connected ? POLLIN : POLLOUT), 0});
        ::poll(fds.data(), static_cast<nfds_t>(fds.size()), 20);

        const auto now = Clock::now();
        for (std::size_t i = 0; i < active.size(); ++i) {
            Probe& p = active[i];
            if (fds[i].revents != 0 && !p.connected) {
                int error = 0;
                socklen_t length = sizeof error;
                ::getsockopt(p.socket.fd(), SOL_SOCKET, SO_ERROR, &error, &length);
                if (error != 0 || ::send(p.socket.fd(), "getVersion\r\n", 12, 0) != 12) {
                    p.finished = true;
                    continue;
                }
                p.connected = true;
                p.deadline = after(std::max(timeout, kReplyTimeout));
            } else if (fds[i].revents != 0) {
                char chunk[64];
                ssize_t got = ::recv(p.socket.fd(), chunk, sizeof chunk, 0);
                if (got > 0) p.received.append(chunk, static_cast<std::size_t>(got));
                if (got <= 0 || p.received.find('\n') != std::string::npos || p.received.size() >= 64) {
                    if (auto version = version_from(p.received)) found.emplace_back(p.host, *version);
                    p.finished = true;
                }
            }
            if (now > p.deadline) p.finished = true;
        }
        std::erase_if(active, [](const Probe& p) { return p.finished; });
    }

    std::ranges::sort(found);
    std::vector<Console> consoles;
    for (const auto& [host, version] : found) consoles.push_back({ipv4_text(host), version});
    return consoles;
}

}  // namespace botbase
