#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "botbase/errors.hpp"
#include "botbase/signals.hpp"
#include "botbase/util.hpp"
#include "commands.hpp"
#include "session.hpp"

using namespace botbase;
using namespace botbase::cli;

namespace {

std::string environment(const char* name) {
    const char* value = std::getenv(name);
    return value ? value : "";
}

std::uint16_t parse_port(const std::string& text) {
    std::uint64_t port = parse_u64(text);
    if (port == 0 || port > 65535) throw UsageError("port must be 1..65535");
    return static_cast<std::uint16_t>(port);
}

GlobalOptions options_from_environment() {
    GlobalOptions options;
    options.host = environment("BOTBASE_HOST");
    options.interface = environment("BOTBASE_INTERFACE");
    options.subnet = environment("BOTBASE_SUBNET");
    if (auto port = environment("BOTBASE_PORT"); !port.empty()) options.port = parse_port(port);
    return options;
}

// Global options come before the command. Returns the index of the command,
// or -1 when the options already answered (--help, --version).
int parse_global_options(const std::vector<std::string>& words, GlobalOptions& options) {
    std::size_t i = 0;
    auto value = [&](const std::string& name) -> const std::string& {
        if (i + 1 >= words.size()) throw UsageError(name + " needs a value");
        return words[++i];
    };

    for (; i < words.size() && words[i].starts_with("-"); ++i) {
        const std::string& word = words[i];
        if (word == "--help" || word == "-h") {
            print_usage(std::cout);
            return -1;
        } else if (word == "--version") {
            std::cout << "botbase " BOTBASE_VERSION "\n";
            return -1;
        } else if (word == "--host") {
            options.host = value(word);
        } else if (word == "--port") {
            options.port = parse_port(value(word));
        } else if (word == "--interface") {
            options.interface = value(word);
        } else if (word == "--subnet") {
            options.subnet = value(word);
        } else if (word == "--timeout") {
            options.timeout = std::stod(value(word));
            if (options.timeout <= 0) throw UsageError("--timeout must be positive");
        } else if (word == "--fast") {
            options.fast = true;
        } else if (word == "--detach") {
            options.detach = true;
        } else if (word == "--no-cache") {
            options.use_cache = false;
        } else if (word == "--quiet" || word == "-q") {
            options.quiet = true;
        } else {
            throw UsageError("unknown option '" + word + "'; options go before the command");
        }
    }
    if (i == words.size()) {
        print_usage(std::cerr);
        return -2;
    }
    return static_cast<int>(i);
}

}  // namespace

int main(int argc, char** argv) {
    install_signal_handlers();
    std::vector<std::string> words(argv + 1, argv + argc);

    GlobalOptions options;
    int command_index;
    try {
        options = options_from_environment();
        command_index = parse_global_options(words, options);
    } catch (const std::exception& error) {
        return report_error(error);
    }
    if (command_index == -1) return 0;
    if (command_index == -2) return static_cast<int>(ExitCode::Usage);

    Session session(options);
    int code = 0;
    bool failed = false;
    try {
        code = run_command(session, {words.begin() + command_index, words.end()});
    } catch (const std::exception& error) {
        code = report_error(error);
        failed = true;
    }
    session.finish(failed);
    return code;
}
