#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "botbase/util.hpp"

namespace botbase {

// How memory is shown on the way out or encoded on the way in.
enum class Format { Hex, Dump, U8, I8, U16, I16, U32, I32, U64, I64, F32, F64 };

std::optional<Format> parse_format(std::string_view name);
std::string_view format_names();
// Byte width of a numeric format, or 0 for hex and dump.
std::size_t format_width(Format format);

Bytes encode_value(std::string_view value, Format format);
std::string show_bytes(const Bytes& data, Format format, std::uint64_t base_address = 0);

// Shortest round-trip form, written the way Python prints floats.
std::string format_double(double value);

}  // namespace botbase
