#include "host_cache.hpp"

#include <cstdlib>
#include <fstream>
#include <system_error>

#include "botbase/util.hpp"

namespace botbase::cli {

std::filesystem::path host_cache_path() {
    if (const char* xdg = std::getenv("XDG_CACHE_HOME"); xdg && *xdg) return std::filesystem::path(xdg) / "botbase" / "host";
    if (const char* home = std::getenv("HOME"); home && *home) return std::filesystem::path(home) / ".cache" / "botbase" / "host";
    return {};
}

std::optional<std::string> read_cached_host() {
    auto path = host_cache_path();
    if (path.empty()) return std::nullopt;
    std::ifstream file(path);
    std::string host;
    if (!std::getline(file, host)) return std::nullopt;
    host = std::string(trim(host));
    if (host.empty()) return std::nullopt;
    return host;
}

void write_cached_host(const std::string& host) noexcept {
    try {
        auto path = host_cache_path();
        if (path.empty()) return;
        std::error_code error;
        std::filesystem::create_directories(path.parent_path(), error);

        auto temporary = path;
        temporary += ".tmp";
        {
            std::ofstream file(temporary, std::ios::trunc);
            file << host << '\n';
            if (!file) return;
        }
        std::filesystem::rename(temporary, path, error);
    } catch (...) {
        // A missing cache only costs a scan next time.
    }
}

}  // namespace botbase::cli
