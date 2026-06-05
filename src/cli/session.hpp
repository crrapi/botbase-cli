#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

#include "botbase/client.hpp"

namespace botbase::cli {

struct GlobalOptions {
    std::string host;
    std::string interface;
    std::string subnet;
    std::uint16_t port = kDefaultPort;
    double timeout = 10.0;
    bool fast = false;
    bool detach = false;
    bool use_cache = true;
    bool quiet = false;
};

// Owns the connection for one run of the CLI, and puts the console back the
// way it found it on the way out.
class Session {
public:
    explicit Session(GlobalOptions options) : options_(std::move(options)) {}

    const GlobalOptions& options() const { return options_; }

    // Connects on first use, finding the console if no host was given.
    Client& client();

    // Status messages go to stderr so stdout stays clean for pipes.
    void note(std::string_view message) const;

    // Drops replies an aborted command left behind.
    void resync() noexcept;
    // resync(), then lets go of any buttons, sticks or touches still held.
    void recover() noexcept;
    // For failures that were reported without an exception reaching main,
    // like a script that stopped on its first error.
    void mark_failed() noexcept { failed_ = true; }
    // Runs once at exit: recovers after a failure, undoes --fast, honours --detach.
    void finish(bool failed) noexcept;

private:
    std::string resolve_host();

    GlobalOptions options_;
    std::unique_ptr<Client> client_;
    bool failed_ = false;
};

}  // namespace botbase::cli
