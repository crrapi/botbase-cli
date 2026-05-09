#pragma once

#include <stdexcept>
#include <string>

namespace botbase {

// The CLI exits with these, so scripts can tell failures apart.
enum class ExitCode : int {
    Ok = 0,
    Failure = 1,
    Usage = 2,
    Connection = 3,
    Timeout = 4,
    NoGame = 5,
    DebuggerBusy = 6,
    MemoryAccess = 7,
    Interrupted = 130,
};

class Error : public std::runtime_error {
public:
    explicit Error(const std::string& message, ExitCode code = ExitCode::Failure)
        : std::runtime_error(message), code_(code) {}

    ExitCode code() const noexcept { return code_; }
    virtual const char* kind() const noexcept { return "Error"; }

private:
    ExitCode code_;
};

#define BOTBASE_DEFINE_ERROR(Name, Code)                                       \
    class Name : public Error {                                                \
    public:                                                                    \
        explicit Name(const std::string& message)                              \
            : Error(message, ExitCode::Code) {}                                \
        const char* kind() const noexcept override { return #Name; }           \
    };

// A bad argument. Nothing was sent to the console.
BOTBASE_DEFINE_ERROR(UsageError, Usage)
// sys-botbase answered with something this client does not understand.
BOTBASE_DEFINE_ERROR(ProtocolError, Failure)
BOTBASE_DEFINE_ERROR(ConnectionError, Connection)
BOTBASE_DEFINE_ERROR(TimeoutError, Timeout)
BOTBASE_DEFINE_ERROR(NoGameRunning, NoGame)
// Another debugger (usually Atmosphere's cheat engine) holds the game.
BOTBASE_DEFINE_ERROR(DebuggerBusy, DebuggerBusy)
// A read failed: unmapped address, no game, or a busy debugger.
BOTBASE_DEFINE_ERROR(MemoryAccessError, MemoryAccess)
BOTBASE_DEFINE_ERROR(Interrupted, Interrupted)

#undef BOTBASE_DEFINE_ERROR

}  // namespace botbase
