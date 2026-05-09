#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "botbase/protocol.hpp"

namespace botbase {

struct Console {
    std::string host;
    std::string version;
};

// The local IPv4 address that routes to the LAN. Sends nothing.
std::string local_ipv4(const std::string& interface = {});
// The /24 around local_ipv4(), e.g. 192.168.1.0/24.
std::string default_subnet(const std::string& interface = {});

// The sys-botbase version at host:port, or nothing. Port 6000 is also X11's,
// so a host only counts when it answers getVersion with a version number.
std::optional<std::string> probe(const std::string& host, std::uint16_t port = kDefaultPort,
                                 const std::string& interface = {}, double timeout = 0.6);

// Probes every host in the subnet in parallel. Results are in address order.
std::vector<Console> discover(const std::string& subnet, std::uint16_t port = kDefaultPort,
                              const std::string& interface = {}, double timeout = 0.6);

}  // namespace botbase
