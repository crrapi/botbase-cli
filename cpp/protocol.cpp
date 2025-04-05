#include "protocol.hpp"
#include <array>
#include <algorithm>

namespace botbase {

namespace {
constexpr std::array<std::string_view, 24> kReply = {
    "peek", "peekMulti", "peekAbsolute", "peekAbsoluteMulti", "peekMain",
    "peekMainMulti", "pointer", "pointerAll", "pointerRelative", "pointerPeek",
    "pointerPeekMulti", "getTitleID", "getTitleVersion", "getSystemLanguage",
    "getMainNsoBase", "getBuildID", "getHeapBase", "isProgramRunning",
    "pixelPeek", "getVersion", "charge", "fdCount", "freezeCount", "game",
};
}

bool is_reply_command(std::string_view word) {
    return std::find(kReply.begin(), kReply.end(), word) != kReply.end();
}

bool is_known_command(std::string_view word) { return is_reply_command(word); }

}
