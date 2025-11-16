#include "signals.hpp"
#include <atomic>
#include <csignal>
#include <stdexcept>

namespace botbase {

namespace {
std::atomic<int> pending{0};
extern "C" void on_signal(int s) { pending.store(s); }
}

void install_signal_handlers() {
    struct sigaction a{};
    a.sa_handler = on_signal;
    sigemptyset(&a.sa_mask);
    sigaction(SIGINT, &a, nullptr);
    sigaction(SIGTERM, &a, nullptr);
    std::signal(SIGPIPE, SIG_IGN);
}

bool interrupted() noexcept { return pending.load() != 0; }

void throw_if_interrupted() {
    if (interrupted()) throw std::runtime_error("interrupted");
}

}
