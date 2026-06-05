#pragma once

#include <exception>
#include <iosfwd>
#include <string>
#include <string_view>
#include <vector>

#include "session.hpp"

namespace botbase::cli {

// Runs one command, e.g. {"click", "A", "B"}, and returns its exit code.
// Scripts run with nested = true, which rules out batch, shell and discover.
int run_command(Session& session, const std::vector<std::string>& words, bool nested = false);

void print_usage(std::ostream& out);
bool print_command_help(std::string_view name, std::ostream& out);

// Prints the error to stderr and returns the exit code that goes with it.
int report_error(const std::exception& error, std::string_view context = {});

}  // namespace botbase::cli
