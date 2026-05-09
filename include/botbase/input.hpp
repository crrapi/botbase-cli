#pragma once

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "botbase/protocol.hpp"

namespace botbase {

// sys-botbase presses A for any button name it does not know, so every name
// goes through here first. Accepts aliases like "up", "+" and "start".
std::string normalize_button(std::string_view name);
std::span<const std::string_view> button_names();

// A HidKeyboardKey name ("Return", "a", "F5") or a number in 4..231.
int key_code(std::string_view key);
int key_code(int code);
std::span<const std::pair<std::string_view, int>> key_names();

// A HidKeyboardModifier bit, by name ("Shift") or number.
int modifier_bit(std::string_view modifier);

struct KeyStroke {
    int code;
    bool shift;
};

// How a character is typed on a US layout, if it can be.
std::optional<KeyStroke> us_layout(char c);

int clamp_stick(long long value);

struct Step {
    enum class Kind { Click, Wait, Press, Release, Stick, Hold };

    Kind kind;
    std::string button{};
    int ms = 0;
    Side side = Side::Left;
    int x = 0;
    int y = 0;

    static Step click(std::string name);
    static Step wait(int duration_ms);
    static Step press(std::string name);
    static Step release(std::string name);
    static Step hold(std::string name, int duration_ms);
    static Step stick(Side which, int to_x, int to_y);

    bool operator==(const Step&) const = default;
};

// clickSeq syntax: A, W250 (wait), +A (press), -A (release), %x,y (left
// stick), &x,y (right stick). Tokens may be separate or comma-joined.
std::vector<Step> parse_sequence(std::span<const std::string> tokens);
std::vector<Step> parse_sequence(std::string_view text);
std::string build_click_seq(std::span<const Step> steps);

}  // namespace botbase
