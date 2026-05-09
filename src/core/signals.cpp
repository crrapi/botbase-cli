#include "botbase/signals.hpp"

#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <thread>

#include "botbase/errors.hpp"

namespace botbase {

namespace {

std::atomic<int> pending_signal{0};
std::atomic<bool> cleaning_up{false};

extern "C" void on_signal(int signal) {
    if (cleaning_up.load()) _exit(128 + signal);
    pending_signal.store(signal);
}

}  // namespace

void install_signal_handlers() {
    struct sigaction action {};
    action.sa_handler = on_signal;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    sigaction(SIGINT, &action, nullptr);
    sigaction(SIGTERM, &action, nullptr);
    std::signal(SIGPIPE, SIG_IGN);
}

bool interrupted() noexcept { return pending_signal.load() != 0; }

void throw_if_interrupted() {
    if (interrupted()) throw Interrupted("interrupted");
}

void clear_interrupt() noexcept { pending_signal.store(0); }

void begin_cleanup() noexcept {
    pending_signal.store(0);
    cleaning_up.store(true);
}

void sleep_ms(double ms) {
    using namespace std::chrono;
    const auto deadline = steady_clock::now() + duration_cast<steady_clock::duration>(duration<double, std::milli>(ms));
    for (;;) {
        throw_if_interrupted();
        auto left = deadline - steady_clock::now();
        if (left <= steady_clock::duration::zero()) return;
        std::this_thread::sleep_for(std::min<steady_clock::duration>(left, milliseconds(10)));
    }
}

}  // namespace botbase
