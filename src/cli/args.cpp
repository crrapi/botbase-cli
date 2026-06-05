#include "args.hpp"

#include <algorithm>
#include <cctype>
#include <format>

#include "botbase/errors.hpp"

namespace botbase::cli {

namespace {

bool looks_like_option(std::string_view word) {
    if (word.starts_with("--")) return word.size() > 2;
    return word.size() >= 2 && word[0] == '-' && std::isalpha(static_cast<unsigned char>(word[1]));
}

std::string plural(std::size_t count, std::string_view noun) {
    return std::format("{} {}{}", count, noun, count == 1 ? "" : "s");
}

}  // namespace

Args::Args(std::string command, std::vector<std::string> words)
    : command_(std::move(command)), words_(std::move(words)) {
    options_end_ = static_cast<std::size_t>(std::ranges::find(words_, "--") - words_.begin());
}

std::size_t Args::find(std::string_view name, std::string_view short_name) const {
    for (std::size_t i = 0; i < options_end_; ++i) {
        std::string_view word = words_[i];
        if (word == name || (!short_name.empty() && word == short_name)) return i;
        if (word.starts_with(name) && word.size() > name.size() && word[name.size()] == '=') return i;
    }
    return words_.size();
}

bool Args::flag(std::string_view name, std::string_view short_name) {
    std::size_t i = find(name, short_name);
    if (i == words_.size()) return false;
    if (words_[i] != name && words_[i] != short_name) throw UsageError(std::format("{} takes no value", name));
    words_.erase(words_.begin() + static_cast<std::ptrdiff_t>(i));
    --options_end_;
    return true;
}

std::optional<std::string> Args::option(std::string_view name, std::string_view short_name) {
    std::size_t i = find(name, short_name);
    if (i == words_.size()) return std::nullopt;

    std::string value;
    std::size_t taken = 1;
    if (auto equals = words_[i].find('='); equals != std::string::npos) {
        value = words_[i].substr(equals + 1);
    } else if (i + 1 < options_end_) {
        value = words_[i + 1];
        taken = 2;
    } else {
        throw UsageError(std::format("{} needs a value", name));
    }
    words_.erase(words_.begin() + static_cast<std::ptrdiff_t>(i),
                 words_.begin() + static_cast<std::ptrdiff_t>(i + taken));
    options_end_ -= taken;
    return value;
}

std::vector<std::string> Args::positionals(std::size_t min, std::size_t max, bool allow_dashes) {
    std::vector<std::string> out;
    for (std::size_t i = 0; i < words_.size(); ++i) {
        if (i == options_end_) continue;
        if (i < options_end_ && looks_like_option(words_[i]) && (!allow_dashes || words_[i].starts_with("--"))) {
            throw UsageError(std::format("unknown option '{}'", words_[i]));
        }
        out.push_back(words_[i]);
    }

    if (out.size() < min || out.size() > max) {
        std::string wanted = min == max ? plural(min, "argument")
                             : max == std::numeric_limits<std::size_t>::max() ? "at least " + plural(min, "argument")
                                                                              : std::format("{} to {} arguments", min, max);
        throw UsageError(std::format("expected {}, got {}", wanted, out.size()));
    }
    return out;
}

}  // namespace botbase::cli
