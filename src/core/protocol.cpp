#include "botbase/protocol.hpp"

#include <algorithm>
#include <array>
#include <format>

#include "botbase/errors.hpp"
#include "botbase/util.hpp"

namespace botbase {

namespace {

constexpr std::array<std::string_view, 24> kReplyCommands = {
    "peek",          "peekMulti",       "peekAbsolute", "peekAbsoluteMulti", "peekMain",
    "peekMainMulti", "pointer",         "pointerAll",   "pointerRelative",   "pointerPeek",
    "pointerPeekMulti", "getTitleID",   "getTitleVersion", "getSystemLanguage", "getMainNsoBase",
    "getBuildID",    "getHeapBase",     "isProgramRunning", "pixelPeek",      "getVersion",
    "charge",        "fdCount",         "freezeCount",  "game",
};

constexpr std::array<std::string_view, 26> kSilentCommands = {
    "poke",        "pokeAbsolute", "pokeMain",    "pointerPoke", "click",       "clickSeq",
    "clickCancel", "press",        "release",     "setStick",    "detachController",
    "configure",   "freeze",       "unFreeze",    "freezeClear", "freezePause", "freezeUnpause",
    "touch",       "touchHold",    "touchDraw",   "touchCancel", "key",         "keyMod",
    "keyMulti",    "screenOff",    "screenOn",
};

constexpr std::array<ConfigKey, 9> kConfigKeys = {{
    {"mainLoopSleepTime", 50, "ms the main loop sleeps after each command; sets the per-command latency"},
    {"buttonClickSleepTime", 50, "ms a button is held during click and clickSeq"},
    {"keySleepTime", 25, "ms a keyboard key is held"},
    {"fingerDiameter", 50, "virtual touch diameter"},
    {"pollRate", 17, "ms per touch or key poll, about one 60 Hz frame"},
    {"freezeRate", 3, "ms between freeze rewrites"},
    {"echoCommands", 0, "1 echoes every command back after its output"},
    {"printDebugResultCodes", 0, "1 prints failing libnx Result codes into the reply stream"},
    {"controllerType", 3, "HidDeviceType of the virtual controller; changing it detaches the controller"},
}};

// HidDeviceType values from libnx.
constexpr std::array<ControllerType, 17> kControllerTypes = {{
    {"joycon-right", 1},
    {"joycon-left", 2},
    {"pro", 3},
    {"joycon-left-4", 4},
    {"joycon-right-5", 5},
    {"fullkey-6", 6},
    {"famicom-left", 7},
    {"famicom-right", 8},
    {"nes-left", 9},
    {"nes-right", 10},
    {"snes", 11},
    {"pokeball", 12},
    {"fullkey-13", 13},
    {"fullkey-15", 15},
    {"debugpad", 17},
    {"n64", 22},
    {"genesis", 28},
}};

constexpr std::array<std::string_view, 18> kLanguages = {
    "Japanese", "American English", "French", "German", "Italian", "Spanish",
    "Chinese", "Korean", "Dutch", "Portuguese", "Russian", "Taiwanese",
    "British English", "Canadian French", "Latin American Spanish", "Simplified Chinese",
    "Traditional Chinese", "Brazilian Portuguese",
};

template <typename Range>
bool contains(const Range& range, std::string_view word) {
    return std::ranges::find(range, word) != std::ranges::end(range);
}

}  // namespace

Region parse_region(std::string_view name) {
    if (iequals(name, "heap")) return Region::Heap;
    if (iequals(name, "main")) return Region::Main;
    if (iequals(name, "absolute")) return Region::Absolute;
    throw UsageError(std::format("region must be heap, main or absolute, not '{}'", name));
}

std::string_view region_name(Region region) {
    switch (region) {
        case Region::Heap: return "heap";
        case Region::Main: return "main";
        case Region::Absolute: return "absolute";
    }
    return "heap";
}

Side parse_side(std::string_view name) {
    if (iequals(name, "left")) return Side::Left;
    if (iequals(name, "right")) return Side::Right;
    throw UsageError(std::format("stick must be left or right, not '{}'", name));
}

std::string_view side_name(Side side) { return side == Side::Left ? "LEFT" : "RIGHT"; }

bool is_reply_command(std::string_view word) { return contains(kReplyCommands, word); }

bool is_known_command(std::string_view word) {
    return is_reply_command(word) || contains(kSilentCommands, word);
}

std::span<const ConfigKey> config_keys() { return kConfigKeys; }

const ConfigKey* find_config_key(std::string_view name) {
    auto it = std::ranges::find(kConfigKeys, name, &ConfigKey::name);
    return it == kConfigKeys.end() ? nullptr : &*it;
}

std::span<const ControllerType> controller_types() { return kControllerTypes; }

int parse_controller_type(std::string_view text) {
    for (const auto& type : kControllerTypes) {
        if (iequals(type.name, text)) return type.value;
    }
    try {
        return parse_int(text);
    } catch (const UsageError&) {
        std::string names;
        for (const auto& type : kControllerTypes) names += std::format("{}{}", names.empty() ? "" : ", ", type.name);
        throw UsageError(std::format("unknown controller type '{}'; use a number or one of: {}", text, names));
    }
}

std::string language_name(int code) {
    if (code >= 0 && static_cast<std::size_t>(code) < kLanguages.size()) return std::string(kLanguages[static_cast<std::size_t>(code)]);
    return std::format("unknown ({})", code);
}

}  // namespace botbase
