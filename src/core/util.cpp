#include "botbase/util.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <format>
#include <limits>

#include "botbase/errors.hpp"

namespace botbase {

namespace {

constexpr std::uint64_t kI64Max = static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());

int digit_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'z') return c - 'a' + 10;
    if (c >= 'A' && c <= 'Z') return c - 'A' + 10;
    return 99;
}

constexpr std::array<std::int8_t, 256> make_hex_table() {
    std::array<std::int8_t, 256> table{};
    table.fill(-1);
    for (int i = 0; i < 10; ++i) table['0' + i] = static_cast<std::int8_t>(i);
    for (int i = 0; i < 6; ++i) {
        table['a' + i] = static_cast<std::int8_t>(10 + i);
        table['A' + i] = static_cast<std::int8_t>(10 + i);
    }
    return table;
}

constexpr auto kHexTable = make_hex_table();

std::string_view strip_hex_prefix(std::string_view text) {
    if (text.size() >= 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) text.remove_prefix(2);
    return text;
}

}  // namespace

std::int64_t Integer::as_i64() const {
    if (!negative) {
        if (magnitude > kI64Max) throw UsageError(std::format("{} does not fit a signed 64-bit value", magnitude));
        return static_cast<std::int64_t>(magnitude);
    }
    if (magnitude > kI64Max + 1) throw UsageError(std::format("-{} does not fit a signed 64-bit value", magnitude));
    if (magnitude == kI64Max + 1) return std::numeric_limits<std::int64_t>::min();
    return -static_cast<std::int64_t>(magnitude);
}

std::uint64_t Integer::as_u64() const {
    if (negative) throw UsageError(std::format("-{} must not be negative", magnitude));
    return magnitude;
}

int Integer::as_int() const {
    std::int64_t value = as_i64();
    if (value < std::numeric_limits<int>::min() || value > std::numeric_limits<int>::max()) {
        throw UsageError(std::format("{} is out of range", value));
    }
    return static_cast<int>(value);
}

Integer parse_integer(std::string_view text) {
    auto fail = [&] { return UsageError(std::format("not a number: '{}'", text)); };

    std::string_view digits = trim(text);
    Integer result;
    if (!digits.empty() && (digits[0] == '+' || digits[0] == '-')) {
        result.negative = digits[0] == '-';
        digits.remove_prefix(1);
    }

    int base = 10;
    if (digits.size() >= 2 && digits[0] == '0') {
        switch (digits[1]) {
            case 'x': case 'X': base = 16; break;
            case 'b': case 'B': base = 2; break;
            case 'o': case 'O': base = 8; break;
            default: break;
        }
        if (base != 10) digits.remove_prefix(2);
    }
    if (digits.empty() || digits.front() == '_' || digits.back() == '_') throw fail();

    std::uint64_t value = 0;
    char previous = 0;
    for (char c : digits) {
        if (c == '_') {
            if (previous == '_') throw fail();
            previous = c;
            continue;
        }
        int digit = digit_value(c);
        if (digit >= base) throw fail();
        if (value > (std::numeric_limits<std::uint64_t>::max() - static_cast<std::uint64_t>(digit)) / static_cast<std::uint64_t>(base)) {
            throw UsageError(std::format("{} is too large", text));
        }
        value = value * static_cast<std::uint64_t>(base) + static_cast<std::uint64_t>(digit);
        previous = c;
    }

    result.magnitude = value;
    if (value == 0) result.negative = false;
    return result;
}

std::string hex_arg(std::int64_t value) {
    if (value >= 0) return std::format("{:#x}", value);
    auto magnitude = static_cast<std::uint64_t>(-(value + 1)) + 1;
    return std::format("-{:#x}", magnitude);
}

std::string hex_arg_unsigned(std::uint64_t value) { return std::format("{:#x}", value); }

std::string to_hex(const Bytes& data, bool uppercase) {
    const char* digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
    std::string out(data.size() * 2, '\0');
    for (std::size_t i = 0; i < data.size(); ++i) {
        out[2 * i] = digits[data[i] >> 4];
        out[2 * i + 1] = digits[data[i] & 0xF];
    }
    return out;
}

std::optional<Bytes> decode_hex(std::string_view hex) {
    if (hex.size() % 2 != 0) return std::nullopt;
    Bytes out(hex.size() / 2);
    for (std::size_t i = 0; i < out.size(); ++i) {
        int high = kHexTable[static_cast<unsigned char>(hex[2 * i])];
        int low = kHexTable[static_cast<unsigned char>(hex[2 * i + 1])];
        if (high < 0 || low < 0) return std::nullopt;
        out[i] = static_cast<std::uint8_t>(high << 4 | low);
    }
    return out;
}

Bytes parse_hex_bytes(std::string_view text) {
    std::string hex;
    for (char c : strip_hex_prefix(trim(text))) {
        if (c != ' ' && c != '_') hex += c;
    }
    if (hex.size() % 2 != 0) hex.insert(hex.begin(), '0');
    auto bytes = decode_hex(hex);
    if (!bytes) throw UsageError(std::format("not hex bytes: '{}'", text));
    if (bytes->empty()) throw UsageError("data must not be empty");
    return *bytes;
}

std::optional<std::uint64_t> parse_hex_number(std::string_view text) {
    std::string_view digits = strip_hex_prefix(trim(text));
    if (digits.empty()) return std::nullopt;
    while (digits.size() > 1 && digits.front() == '0') digits.remove_prefix(1);
    if (digits.size() > 16) return std::nullopt;

    std::uint64_t value = 0;
    for (char c : digits) {
        int digit = kHexTable[static_cast<unsigned char>(c)];
        if (digit < 0) return std::nullopt;
        value = value << 4 | static_cast<std::uint64_t>(digit);
    }
    return value;
}

bool is_version_string(std::string_view text) {
    if (text.empty() || !std::isdigit(static_cast<unsigned char>(text.front())) ||
        !std::isdigit(static_cast<unsigned char>(text.back()))) {
        return false;
    }
    char previous = 0;
    for (char c : text) {
        if (c == '.' && previous == '.') return false;
        if (c != '.' && !std::isdigit(static_cast<unsigned char>(c))) return false;
        previous = c;
    }
    return true;
}

std::string to_lower(std::string_view text) {
    std::string out(text);
    std::ranges::transform(out, out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

std::string to_upper(std::string_view text) {
    std::string out(text);
    std::ranges::transform(out, out.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return out;
}

bool iequals(std::string_view a, std::string_view b) {
    return a.size() == b.size() &&
           std::equal(a.begin(), a.end(), b.begin(), [](unsigned char x, unsigned char y) {
               return std::tolower(x) == std::tolower(y);
           });
}

std::string_view trim(std::string_view text) {
    auto space = [](char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
    while (!text.empty() && space(text.front())) text.remove_prefix(1);
    while (!text.empty() && space(text.back())) text.remove_suffix(1);
    return text;
}

std::string json_quote(std::string_view text) {
    std::string out = "\"";
    for (char c : text) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    out += std::format("\\u{:04x}", static_cast<unsigned char>(c));
                } else {
                    out += c;
                }
        }
    }
    return out + "\"";
}

std::vector<std::vector<std::string>> split_commands(std::string_view script) {
    std::vector<std::vector<std::string>> commands;
    std::vector<std::string> words;
    std::string word;
    bool in_word = false;

    auto end_word = [&] {
        if (in_word) words.push_back(std::move(word));
        word.clear();
        in_word = false;
    };
    auto end_command = [&] {
        end_word();
        if (!words.empty()) commands.push_back(std::move(words));
        words.clear();
    };

    for (std::size_t i = 0; i < script.size(); ++i) {
        char c = script[i];
        switch (c) {
            case ' ': case '\t': case '\r':
                end_word();
                break;
            case '\n': case ';':
                end_command();
                break;
            case '#':
                if (in_word) {
                    word += c;
                } else {
                    while (i + 1 < script.size() && script[i + 1] != '\n') ++i;
                }
                break;
            case '\'': {
                auto close = script.find('\'', i + 1);
                if (close == std::string_view::npos) throw UsageError("unterminated ' quote");
                word.append(script.substr(i + 1, close - i - 1));
                in_word = true;
                i = close;
                break;
            }
            case '"': {
                in_word = true;
                for (++i;; ++i) {
                    if (i >= script.size()) throw UsageError("unterminated \" quote");
                    if (script[i] == '"') break;
                    if (script[i] == '\\' && i + 1 < script.size() &&
                        std::string_view("\"\\$`\n").find(script[i + 1]) != std::string_view::npos) {
                        ++i;
                    }
                    word += script[i];
                }
                break;
            }
            case '\\':
                if (i + 1 < script.size()) {
                    ++i;
                    if (script[i] != '\n') {
                        word += script[i];
                        in_word = true;
                    }
                }
                break;
            default:
                word += c;
                in_word = true;
        }
    }
    end_command();
    return commands;
}

}  // namespace botbase
