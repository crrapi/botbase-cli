#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace botbase {

using Bytes = std::vector<std::uint8_t>;

// A parsed integer literal. Kept as sign and magnitude so the full unsigned
// 64-bit range and negative offsets both fit.
struct Integer {
    bool negative = false;
    std::uint64_t magnitude = 0;

    std::int64_t as_i64() const;
    std::uint64_t as_u64() const;
    int as_int() const;
};

// Decimal, 0x hex, 0b binary or 0o octal, with an optional sign and
// underscores between digits.
Integer parse_integer(std::string_view text);
inline std::int64_t parse_i64(std::string_view text) { return parse_integer(text).as_i64(); }
inline std::uint64_t parse_u64(std::string_view text) { return parse_integer(text).as_u64(); }
inline int parse_int(std::string_view text) { return parse_integer(text).as_int(); }

// The number format sys-botbase's parsers want: 0x hex, sign in front.
std::string hex_arg(std::int64_t value);
std::string hex_arg_unsigned(std::uint64_t value);

std::string to_hex(const Bytes& data, bool uppercase = true);
std::optional<Bytes> decode_hex(std::string_view hex);
// User-typed bytes: optional 0x, spaces allowed, an odd length gets a leading 0.
Bytes parse_hex_bytes(std::string_view text);
// A hex number from a reply line, with or without 0x.
std::optional<std::uint64_t> parse_hex_number(std::string_view text);

bool is_version_string(std::string_view text);

std::string to_lower(std::string_view text);
std::string to_upper(std::string_view text);
bool iequals(std::string_view a, std::string_view b);
std::string_view trim(std::string_view text);

std::string json_quote(std::string_view text);

// Shell-style splitting: quotes, backslashes and # comments. Newlines and
// semicolons end a command.
std::vector<std::vector<std::string>> split_commands(std::string_view script);

}  // namespace botbase
