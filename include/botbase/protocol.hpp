#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace botbase {

inline constexpr std::uint16_t kDefaultPort = 6000;

// MAX_LINE_LENGTH in sys-botbase's util.h. It reads into a buffer this size
// without a bounds check, so a longer line corrupts its memory.
inline constexpr std::size_t kMaxLineLength = 344 * 32 * 2;

inline constexpr int kScreenWidth = 1280;
inline constexpr int kScreenHeight = 720;
inline constexpr int kStickMin = -0x8000;
inline constexpr int kStickMax = 0x7FFF;

enum class Region { Heap, Main, Absolute };
enum class Side { Left, Right };

struct Point {
    int x;
    int y;
};

Region parse_region(std::string_view name);
std::string_view region_name(Region region);
Side parse_side(std::string_view name);
std::string_view side_name(Side side);

// Read commands answer with exactly one line. Everything else is silent.
bool is_reply_command(std::string_view word);
bool is_known_command(std::string_view word);

struct ConfigKey {
    std::string_view name;
    int default_value;
    std::string_view help;
};

std::span<const ConfigKey> config_keys();
const ConfigKey* find_config_key(std::string_view name);

struct ControllerType {
    std::string_view name;
    int value;
};

std::span<const ControllerType> controller_types();
// Accepts a name from controller_types() or a raw HidDeviceType number.
int parse_controller_type(std::string_view text);

std::string language_name(int code);

}  // namespace botbase
