#include "botbase/values.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <format>
#include <limits>

#include "botbase/errors.hpp"

namespace botbase {

namespace {

struct FormatInfo {
    std::string_view name;
    Format format;
    std::size_t width;
    bool is_signed;
};

constexpr std::array<FormatInfo, 12> kFormats = {{
    {"hex", Format::Hex, 0, false},   {"dump", Format::Dump, 0, false},
    {"u8", Format::U8, 1, false},     {"i8", Format::I8, 1, true},
    {"u16", Format::U16, 2, false},   {"i16", Format::I16, 2, true},
    {"u32", Format::U32, 4, false},   {"i32", Format::I32, 4, true},
    {"u64", Format::U64, 8, false},   {"i64", Format::I64, 8, true},
    {"f32", Format::F32, 4, false},   {"f64", Format::F64, 8, false},
}};

const FormatInfo& info(Format format) {
    for (const auto& entry : kFormats) {
        if (entry.format == format) return entry;
    }
    return kFormats[0];
}

Bytes little_endian(std::uint64_t value, std::size_t width) {
    Bytes out(width);
    for (std::size_t i = 0; i < width; ++i) out[i] = static_cast<std::uint8_t>(value >> (8 * i));
    return out;
}

std::uint64_t read_little_endian(const Bytes& data, std::size_t width) {
    std::uint64_t value = 0;
    for (std::size_t i = 0; i < width; ++i) value |= static_cast<std::uint64_t>(data[i]) << (8 * i);
    return value;
}

double parse_double(std::string_view text) {
    std::string copy(text);
    char* end = nullptr;
    double value = std::strtod(copy.c_str(), &end);
    if (copy.empty() || end != copy.c_str() + copy.size()) throw UsageError(std::format("not a number: '{}'", text));
    return value;
}

std::string dump(const Bytes& data, std::uint64_t base) {
    std::string out;
    for (std::size_t row = 0; row < data.size(); row += 16) {
        std::string hex;
        std::string text;
        for (std::size_t i = row; i < std::min(row + 16, data.size()); ++i) {
            hex += std::format("{}{:02x}", hex.empty() ? "" : " ", data[i]);
            text += (data[i] >= 32 && data[i] < 127) ? static_cast<char>(data[i]) : '.';
        }
        if (!out.empty()) out += '\n';
        out += std::format("{:012x}  {:<47}  {}", base + row, hex, text);
    }
    return out;
}

}  // namespace

std::optional<Format> parse_format(std::string_view name) {
    for (const auto& entry : kFormats) {
        if (entry.name == name) return entry.format;
    }
    return std::nullopt;
}

std::string_view format_names() { return "hex, dump, u8, i8, u16, i16, u32, i32, u64, i64, f32, f64"; }

std::size_t format_width(Format format) { return info(format).width; }

Bytes encode_value(std::string_view value, Format format) {
    const FormatInfo& fmt = info(format);
    switch (format) {
        case Format::Hex:
            return parse_hex_bytes(value);
        case Format::Dump:
            throw UsageError("dump is an output format only");
        case Format::F32: {
            auto bits = std::bit_cast<std::uint32_t>(static_cast<float>(parse_double(value)));
            return little_endian(bits, 4);
        }
        case Format::F64:
            return little_endian(std::bit_cast<std::uint64_t>(parse_double(value)), 8);
        default:
            break;
    }

    const unsigned bits = static_cast<unsigned>(fmt.width * 8);
    Integer number = parse_integer(value);
    if (fmt.is_signed) {
        std::int64_t v = number.as_i64();
        std::int64_t min = bits == 64 ? std::numeric_limits<std::int64_t>::min() : -(std::int64_t{1} << (bits - 1));
        std::int64_t max = bits == 64 ? std::numeric_limits<std::int64_t>::max() : (std::int64_t{1} << (bits - 1)) - 1;
        if (v < min || v > max) throw UsageError(std::format("{} does not fit {}", value, fmt.name));
        return little_endian(static_cast<std::uint64_t>(v), fmt.width);
    }
    std::uint64_t v = number.as_u64();
    if (bits < 64 && v >> bits != 0) throw UsageError(std::format("{} does not fit {}", value, fmt.name));
    return little_endian(v, fmt.width);
}

std::string show_bytes(const Bytes& data, Format format, std::uint64_t base_address) {
    const FormatInfo& fmt = info(format);
    if (format == Format::Hex) return to_hex(data);
    if (format == Format::Dump) return dump(data, base_address);

    if (data.size() < fmt.width) {
        throw UsageError(std::format("{} needs {} bytes, got {}", fmt.name, fmt.width, data.size()));
    }
    std::uint64_t raw = read_little_endian(data, fmt.width);
    switch (format) {
        case Format::F32: return format_double(std::bit_cast<float>(static_cast<std::uint32_t>(raw)));
        case Format::F64: return format_double(std::bit_cast<double>(raw));
        default: break;
    }
    if (fmt.is_signed) {
        const unsigned shift = static_cast<unsigned>(64 - fmt.width * 8);
        auto value = static_cast<std::int64_t>(raw << shift) >> shift;
        return std::to_string(value);
    }
    return std::to_string(raw);
}

std::string format_double(double value) {
    if (std::isnan(value)) return "nan";
    if (std::isinf(value)) return value < 0 ? "-inf" : "inf";

    char buffer[64];
    auto result = std::to_chars(buffer, buffer + sizeof buffer, value, std::chars_format::scientific);
    std::string scientific(buffer, result.ptr);

    std::string sign;
    if (scientific.front() == '-') {
        sign = "-";
        scientific.erase(0, 1);
    }
    auto e = scientific.find('e');
    int exponent = std::stoi(scientific.substr(e + 1));
    std::string digits;
    for (char c : scientific.substr(0, e)) {
        if (c != '.') digits += c;
    }

    // Python switches to exponent notation outside 1e-4 .. 1e16.
    if (exponent < -4 || exponent >= 16) {
        std::string out = digits.substr(0, 1);
        if (digits.size() > 1) out += "." + digits.substr(1);
        return sign + out + std::format("e{}{:02}", exponent < 0 ? '-' : '+', std::abs(exponent));
    }
    if (exponent < 0) return sign + "0." + std::string(static_cast<std::size_t>(-exponent - 1), '0') + digits;

    auto integer_digits = static_cast<std::size_t>(exponent + 1);
    if (digits.size() <= integer_digits) return sign + digits + std::string(integer_digits - digits.size(), '0') + ".0";
    return sign + digits.substr(0, integer_digits) + "." + digits.substr(integer_digits);
}

}  // namespace botbase
