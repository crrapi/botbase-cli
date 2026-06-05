#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace botbase::cli {

// The last console found by a scan, so the next run can skip it.
// Lives in $XDG_CACHE_HOME/botbase/host, or ~/.cache/botbase/host.
std::filesystem::path host_cache_path();
std::optional<std::string> read_cached_host();
void write_cached_host(const std::string& host) noexcept;

}  // namespace botbase::cli
