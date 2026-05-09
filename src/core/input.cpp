#include "botbase/input.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <format>

#include "botbase/errors.hpp"
#include "botbase/util.hpp"

namespace botbase {

namespace {

constexpr std::array<std::string_view, 24> kButtons = {
    "A",     "B",     "X",      "Y",      "L",    "R",    "ZL",   "ZR",
    "PLUS",  "MINUS", "LSTICK", "RSTICK", "DUP",  "DDOWN", "DLEFT", "DRIGHT",
    "DU",    "DD",    "DL",     "DR",     "HOME", "CAPTURE",
    "PALMA",   // the Poke Ball Plus button
    "UNUSED",  // a dummy input games ignore; handy for waking the controller
};

constexpr std::array<std::pair<std::string_view, std::string_view>, 13> kButtonAliases = {{
    {"UP", "DUP"},     {"DOWN", "DDOWN"},  {"LEFT", "DLEFT"},  {"RIGHT", "DRIGHT"},
    {"+", "PLUS"},     {"-", "MINUS"},     {"START", "PLUS"},  {"SELECT", "MINUS"},
    {"LS", "LSTICK"},  {"RS", "RSTICK"},   {"L3", "LSTICK"},   {"R3", "RSTICK"},
    {"SCREENSHOT", "CAPTURE"},
}};

constexpr auto kKeys = [] {
    std::array<std::pair<std::string_view, int>, 120> keys{};
    std::size_t n = 0;
    constexpr std::string_view letters = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    for (int i = 0; i < 26; ++i) keys[n++] = {letters.substr(static_cast<std::size_t>(i), 1), 4 + i};

    constexpr std::array<std::string_view, 10> digits = {"D1", "D2", "D3", "D4", "D5", "D6", "D7", "D8", "D9", "D0"};
    for (int i = 0; i < 10; ++i) keys[n++] = {digits[static_cast<std::size_t>(i)], 30 + i};

    constexpr std::array<std::string_view, 18> named = {
        "Return", "Escape", "Backspace", "Tab", "Space", "Minus", "Plus", "OpenBracket",
        "CloseBracket", "Pipe", "Tilde", "Semicolon", "Quote", "Backquote", "Comma", "Period",
        "Slash", "CapsLock",
    };
    for (int i = 0; i < 18; ++i) keys[n++] = {named[static_cast<std::size_t>(i)], 40 + i};

    constexpr std::array<std::string_view, 24> function = {
        "F1",  "F2",  "F3",  "F4",  "F5",  "F6",  "F7",  "F8",  "F9",  "F10", "F11", "F12",
        "F13", "F14", "F15", "F16", "F17", "F18", "F19", "F20", "F21", "F22", "F23", "F24",
    };
    for (int i = 0; i < 12; ++i) keys[n++] = {function[static_cast<std::size_t>(i)], 58 + i};

    constexpr std::array<std::string_view, 19> navigation = {
        "PrintScreen", "ScrollLock", "Pause", "Insert", "Home", "PageUp", "Delete", "End",
        "PageDown", "RightArrow", "LeftArrow", "DownArrow", "UpArrow", "NumLock",
        "NumPadDivide", "NumPadMultiply", "NumPadSubtract", "NumPadAdd", "NumPadEnter",
    };
    for (int i = 0; i < 19; ++i) keys[n++] = {navigation[static_cast<std::size_t>(i)], 70 + i};

    constexpr std::array<std::string_view, 15> numpad = {
        "NumPad1", "NumPad2", "NumPad3", "NumPad4", "NumPad5", "NumPad6", "NumPad7", "NumPad8",
        "NumPad9", "NumPad0", "NumPadDot", "Backslash", "Application", "Power", "NumPadEquals",
    };
    for (int i = 0; i < 15; ++i) keys[n++] = {numpad[static_cast<std::size_t>(i)], 89 + i};

    for (int i = 12; i < 24; ++i) keys[n++] = {function[static_cast<std::size_t>(i)], 104 + (i - 12)};

    constexpr std::array<std::string_view, 8> modifiers = {
        "LeftControl", "LeftShift", "LeftAlt", "LeftGui", "RightControl", "RightShift", "RightAlt", "RightGui",
    };
    for (int i = 0; i < 8; ++i) keys[n++] = {modifiers[static_cast<std::size_t>(i)], 224 + i};
    return keys;
}();

constexpr std::array<std::pair<std::string_view, int>, 10> kModifierBits = {{
    {"Control", 0}, {"Shift", 1}, {"LeftAlt", 2}, {"RightAlt", 3}, {"Gui", 4},
    {"CapsLock", 8}, {"ScrollLock", 9}, {"NumLock", 10}, {"Katakana", 11}, {"Hiragana", 12},
}};

bool all_digits(std::string_view text) {
    return !text.empty() && std::ranges::all_of(text, [](unsigned char c) { return std::isdigit(c); });
}

}  // namespace

std::string normalize_button(std::string_view name) {
    std::string upper = to_upper(trim(name));
    for (const auto& [alias, button] : kButtonAliases) {
        if (upper == alias) upper = button;
    }
    if (std::ranges::find(kButtons, upper) != kButtons.end()) return upper;

    std::vector<std::string_view> sorted(kButtons.begin(), kButtons.end());
    std::ranges::sort(sorted);
    std::string valid;
    for (auto button : sorted) valid += std::format("{}{}", valid.empty() ? "" : " ", button);
    throw UsageError(std::format("unknown button '{}' (sys-botbase would press A). Valid: {}", name, valid));
}

std::span<const std::string_view> button_names() { return kButtons; }

int key_code(int code) {
    if (code < 4 || code > 231) throw UsageError(std::format("key code {} is outside 4..231, which sys-botbase ignores", code));
    return code;
}

int key_code(std::string_view key) {
    if (all_digits(key)) return key_code(parse_int(key));
    for (const auto& [name, code] : kKeys) {
        if (iequals(name, key)) return code;
    }
    throw UsageError(std::format("unknown keyboard key '{}'", key));
}

std::span<const std::pair<std::string_view, int>> key_names() { return kKeys; }

int modifier_bit(std::string_view modifier) {
    if (all_digits(modifier)) return parse_int(modifier);
    for (const auto& [name, bit] : kModifierBits) {
        if (iequals(name, modifier)) return bit;
    }
    throw UsageError(std::format("unknown keyboard modifier '{}'", modifier));
}

std::optional<KeyStroke> us_layout(char c) {
    if (c >= 'a' && c <= 'z') return KeyStroke{4 + (c - 'a'), false};
    if (c >= 'A' && c <= 'Z') return KeyStroke{4 + (c - 'A'), true};
    if (c >= '1' && c <= '9') return KeyStroke{30 + (c - '1'), false};
    if (c == '0') return KeyStroke{39, false};

    constexpr std::string_view unshifted = " \n\t-=[]\\;'`,./";
    constexpr std::array<int, 14> unshifted_codes = {44, 40, 43, 45, 46, 47, 48, 49, 51, 52, 53, 54, 55, 56};
    if (auto i = unshifted.find(c); i != std::string_view::npos) return KeyStroke{unshifted_codes[i], false};

    constexpr std::string_view shifted = "!@#$%^&*()_+{}|:\"~<>?";
    constexpr std::array<int, 21> shifted_codes = {30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 45,
                                                   46, 47, 48, 49, 51, 52, 53, 54, 55, 56};
    if (auto i = shifted.find(c); i != std::string_view::npos) return KeyStroke{shifted_codes[i], true};

    return std::nullopt;
}

int clamp_stick(long long value) {
    return static_cast<int>(std::clamp<long long>(value, kStickMin, kStickMax));
}

Step Step::click(std::string name) { return {.kind = Kind::Click, .button = std::move(name)}; }
Step Step::wait(int duration_ms) { return {.kind = Kind::Wait, .ms = duration_ms}; }
Step Step::press(std::string name) { return {.kind = Kind::Press, .button = std::move(name)}; }
Step Step::release(std::string name) { return {.kind = Kind::Release, .button = std::move(name)}; }
Step Step::hold(std::string name, int duration_ms) {
    return {.kind = Kind::Hold, .button = std::move(name), .ms = duration_ms};
}
Step Step::stick(Side which, int to_x, int to_y) {
    return {.kind = Kind::Stick, .side = which, .x = clamp_stick(to_x), .y = clamp_stick(to_y)};
}

std::vector<Step> parse_sequence(std::span<const std::string> tokens) {
    std::vector<std::string_view> parts;
    for (const auto& token : tokens) {
        std::string_view rest = token;
        for (;;) {
            auto comma = rest.find(',');
            parts.push_back(rest.substr(0, comma));
            if (comma == std::string_view::npos) break;
            rest.remove_prefix(comma + 1);
        }
    }

    std::vector<Step> steps;
    bool awaiting_y = false;
    for (std::string_view part : parts) {
        if (part.empty()) continue;
        if (awaiting_y) {
            steps.back().y = clamp_stick(parse_i64(part));
            awaiting_y = false;
        } else if ((part[0] == 'W' || part[0] == 'w') && all_digits(part.substr(1))) {
            steps.push_back(Step::wait(parse_int(part.substr(1))));
        } else if (part[0] == '+') {
            steps.push_back(Step::press(normalize_button(part.substr(1))));
        } else if (part[0] == '-') {
            steps.push_back(Step::release(normalize_button(part.substr(1))));
        } else if (part[0] == '%' || part[0] == '&') {
            Side side = part[0] == '%' ? Side::Left : Side::Right;
            steps.push_back(Step::stick(side, clamp_stick(parse_i64(part.substr(1))), 0));
            awaiting_y = true;
        } else {
            steps.push_back(Step::click(normalize_button(part)));
        }
    }
    if (awaiting_y) throw UsageError("a stick step needs both x and y, like %0,32767");
    return steps;
}

std::vector<Step> parse_sequence(std::string_view text) {
    std::string token(text);
    return parse_sequence(std::span<const std::string>(&token, 1));
}

std::string build_click_seq(std::span<const Step> steps) {
    std::string out;
    for (const Step& step : steps) {
        if (!out.empty()) out += ',';
        switch (step.kind) {
            case Step::Kind::Click: out += normalize_button(step.button); break;
            case Step::Kind::Wait: out += std::format("W{}", step.ms); break;
            case Step::Kind::Press: out += "+" + normalize_button(step.button); break;
            case Step::Kind::Release: out += "-" + normalize_button(step.button); break;
            case Step::Kind::Stick:
                out += std::format("{}{},{}", step.side == Side::Left ? '%' : '&', clamp_stick(step.x), clamp_stick(step.y));
                break;
            case Step::Kind::Hold:
                throw UsageError("clickSeq has no hold step; use +BUTTON, Wms, -BUTTON");
        }
    }
    return out;
}

}  // namespace botbase
