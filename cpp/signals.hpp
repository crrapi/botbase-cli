#pragma once

namespace botbase {

void install_signal_handlers();
bool interrupted() noexcept;
void throw_if_interrupted();

}
