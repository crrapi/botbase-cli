#include "input.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <stdexcept>

namespace botbase {

namespace {
constexpr std::array<std::string_view, 22> kButtons = {
    "A", "B", "X", "Y", "L", "R", "ZL", "ZR",
    "PLUS", "MINUS", "LSTICK", "RSTICK",
    "DUP", "DDOWN", "DLEFT", "DRIGHT",
    "DU", "DD", "DL", "DR", "HOME", "CAPTURE",
};
}

std::string normalize_button(std::string_view name) {
    std::string upper(name);
    std::transform(upper.begin(), upper.end(), upper.begin(),
                   [](unsigned char c) { return std::toupper(c); });
    if (upper == "UP") upper = "DUP";
    else if (upper == "DOWN") upper = "DDOWN";
    else if (upper == "LEFT") upper = "DLEFT";
    else if (upper == "RIGHT") upper = "DRIGHT";
    else if (upper == "+") upper = "PLUS";
    else if (upper == "-") upper = "MINUS";
    if (std::find(kButtons.begin(), kButtons.end(), upper) == kButtons.end())
        throw std::runtime_error("unknown button " + std::string(name));
    return upper;
}

}
