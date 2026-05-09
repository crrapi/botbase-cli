#pragma once

namespace botbase {

// SIGINT and SIGTERM set a flag that every wait in this library checks, so a
// Ctrl-C surfaces as an Interrupted exception at a safe point instead of
// killing the process with a button still held. SIGPIPE is ignored.
void install_signal_handlers();

bool interrupted() noexcept;
void throw_if_interrupted();
void clear_interrupt() noexcept;

// Call before undoing inputs on the way out. A second signal from here on
// exits right away.
void begin_cleanup() noexcept;

// Sleeps, but wakes up early (and throws) on an interrupt.
void sleep_ms(double ms);

}  // namespace botbase
