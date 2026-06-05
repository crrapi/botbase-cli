#pragma once

#include <cstddef>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace botbase::cli {

// One command's arguments. Options are pulled out first; what remains are
// the positionals. Anything after "--" is always positional.
class Args {
public:
    Args(std::string command, std::vector<std::string> words);

    const std::string& command() const { return command_; }

    bool flag(std::string_view name, std::string_view short_name = {});
    std::optional<std::string> option(std::string_view name, std::string_view short_name = {});

    // Words like "-L" count as unknown options unless allow_dashes is set,
    // which seq needs for its release steps. Negative numbers are always fine.
    std::vector<std::string> positionals(std::size_t min, std::size_t max = std::numeric_limits<std::size_t>::max(),
                                         bool allow_dashes = false);
    std::vector<std::string> exactly(std::size_t count) { return positionals(count, count); }
    void none() { positionals(0, 0); }

private:
    std::size_t find(std::string_view name, std::string_view short_name) const;

    std::string command_;
    std::vector<std::string> words_;
    std::size_t options_end_;
};

}  // namespace botbase::cli
