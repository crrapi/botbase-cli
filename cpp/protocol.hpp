#pragma once
#include <string_view>

namespace botbase {

inline constexpr std::uint16_t kDefaultPort = 6000;

// MAX_LINE_LENGTH from sys-botbase util.h. no bounds check on the other end
inline constexpr std::size_t kMaxLineLength = 344 * 32 * 2;

bool is_reply_command(std::string_view word);
bool is_known_command(std::string_view word);

}
