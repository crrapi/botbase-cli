#include "commands.hpp"

#include <unistd.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <ctime>
#include <format>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>

#include "args.hpp"
#include "botbase/discovery.hpp"
#include "botbase/errors.hpp"
#include "botbase/signals.hpp"
#include "botbase/values.hpp"
#include "host_cache.hpp"

namespace botbase::cli {

namespace {

using Handler = int (*)(Session&, Args&);

struct Command {
    std::string_view name;
    std::string_view group;
    std::string_view usage;
    std::string_view summary;
    Handler run;
    bool needs_top_level = false;
};

// ---- Shared helpers -------------------------------------------------------

Region region_option(Args& args) {
    auto name = args.option("--region");
    return name ? parse_region(*name) : Region::Heap;
}

Format format_option(Args& args, Format fallback, bool allow_dump = true) {
    auto name = args.option("--as");
    if (!name) return fallback;
    auto format = parse_format(*name);
    if (!format || (!allow_dump && *format == Format::Dump)) {
        throw UsageError(std::format("--as must be one of: {}", format_names()));
    }
    return *format;
}

int ms_argument(std::string_view text) {
    int ms = parse_int(text);
    if (ms < 0) throw UsageError("a duration cannot be negative");
    return ms;
}

std::vector<Point> point_pairs(const std::vector<std::string>& words) {
    if (words.size() % 2 != 0) throw UsageError("coordinates come in x y pairs");
    std::vector<Point> points;
    for (std::size_t i = 0; i < words.size(); i += 2) points.push_back({parse_int(words[i]), parse_int(words[i + 1])});
    return points;
}

std::vector<std::int64_t> offsets(const std::vector<std::string>& words) {
    std::vector<std::int64_t> out;
    for (const auto& word : words) out.push_back(parse_i64(word));
    return out;
}

void write_file(const std::string& path, const Bytes& data) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    if (!file) throw Error("cannot write " + path);
}

// "-" means stdout, for piping a screenshot straight into something else.
void save(const std::string& path, const Bytes& data) {
    if (path == "-") {
        std::cout.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
        std::cout.flush();
    } else {
        write_file(path, data);
        std::cout << path << '\n';
    }
}

std::string timestamped(std::string_view prefix, std::string_view extension) {
    std::time_t now = std::time(nullptr);
    std::tm local{};
    localtime_r(&now, &local);
    char stamp[32];
    std::strftime(stamp, sizeof stamp, "%Y%m%d-%H%M%S", &local);
    return std::format("{}-{}.{}", prefix, stamp, extension);
}

// ---- Console --------------------------------------------------------------

int cmd_discover(Session& session, Args& args) {
    auto subnet = args.option("--subnet").value_or(session.options().subnet);
    args.none();
    if (subnet.empty()) subnet = default_subnet(session.options().interface);

    auto found = discover(subnet, session.options().port, session.options().interface);
    for (const auto& console : found) std::cout << console.host << "\tsys-botbase " << console.version << '\n';
    if (found.empty()) {
        session.note("no sys-botbase on " + subnet);
        return static_cast<int>(ExitCode::Connection);
    }
    if (found.size() == 1 && session.options().use_cache) write_cached_host(found[0].host);
    return 0;
}

struct InfoField {
    std::string name;
    std::string json;
    std::string text;
};

std::vector<InfoField> info_fields(const ConsoleInfo& info) {
    auto string = [](const std::string& value) { return InfoField{"", json_quote(value), value}; };
    auto null = [] { return InfoField{"", "null", ""}; };

    std::vector<InfoField> fields;
    auto add = [&](std::string name, InfoField field) {
        field.name = std::move(name);
        fields.push_back(std::move(field));
    };

    add("version", string(info.version));
    add("charge", {"", std::to_string(info.charge), std::to_string(info.charge)});
    add("language", string(info.language));

    const auto& game = info.game;
    if (!game) {
        for (auto name : {"title_id", "build_id", "title_version", "main_nso_base", "heap_base", "name", "memory_access"}) {
            add(name, null());
        }
        return fields;
    }

    add("title_id", string(std::format("{:016X}", game->title_id)));
    add("build_id", string(game->build_id));
    add("title_version", {"", std::to_string(game->title_version), std::to_string(game->title_version)});
    add("main_nso_base", {"", std::to_string(game->main_nso_base), std::format("{:#x}", game->main_nso_base)});
    if (game->heap_base) {
        add("heap_base", {"", std::to_string(*game->heap_base), std::format("{:#x}", *game->heap_base)});
    } else {
        add("heap_base", null());
    }
    add("name", game->name ? string(*game->name) : null());
    add("memory_access", game->heap_base ? InfoField{"", "true", "yes"} : InfoField{"", "false", "no (debugger busy)"});
    return fields;
}

int cmd_info(Session& session, Args& args) {
    bool json = args.flag("--json");
    args.none();
    auto fields = info_fields(session.client().info());

    if (json) {
        std::cout << "{\n";
        for (std::size_t i = 0; i < fields.size(); ++i) {
            std::cout << std::format(" \"{}\": {}{}\n", fields[i].name, fields[i].json, i + 1 < fields.size() ? "," : "");
        }
        std::cout << "}\n";
    } else {
        for (const auto& field : fields) {
            std::string line = std::format("{:<14} {}", field.name, field.text);
            std::cout << trim(line) << '\n';
        }
    }
    return 0;
}

int cmd_screenshot(Session& session, Args& args) {
    auto path = args.option("--output", "-o").value_or(timestamped("switch", "jpg"));
    args.none();
    save(path, session.client().screenshot());
    return 0;
}

int cmd_screen(Session& session, Args& args) {
    std::string state = to_lower(args.exactly(1)[0]);
    if (state == "on") {
        session.client().screen_on();
    } else if (state == "off") {
        session.client().screen_off();
    } else {
        throw UsageError("screen takes on or off");
    }
    return 0;
}

int cmd_home(Session& session, Args& args) {
    args.none();
    session.client().go_home();
    return 0;
}

int cmd_close(Session& session, Args& args) {
    args.none();
    if (session.client().close_app()) {
        std::cout << "closed\n";
        return 0;
    }
    std::cout << "still running (a controller screen may be eating input)\n";
    return 1;
}

int cmd_running(Session& session, Args& args) {
    bool running = session.client().is_program_running(parse_u64(args.exactly(1)[0]));
    std::cout << (running ? "yes" : "no") << '\n';
    return running ? 0 : 1;
}

int cmd_game(Session& session, Args& args) {
    auto output = args.option("--output", "-o");
    std::string field = to_lower(args.exactly(1)[0]);
    if (field == "icon") {
        save(output.value_or("icon.jpg"), session.client().game_icon());
    } else {
        if (output) throw UsageError("-o only applies to game icon");
        std::cout << session.client().game_field(field) << '\n';
    }
    return 0;
}

// ---- Controller -----------------------------------------------------------

int cmd_click(Session& session, Args& args) {
    int gap = ms_argument(args.option("--gap").value_or("150"));
    std::vector<std::string> buttons;
    for (const auto& word : args.positionals(1)) buttons.push_back(normalize_button(word));

    for (std::size_t i = 0; i < buttons.size(); ++i) {
        if (i > 0 && gap > 0) sleep_ms(gap);
        session.client().click(buttons[i]);
    }
    return 0;
}

int cmd_press(Session& session, Args& args) {
    session.client().press(args.exactly(1)[0]);
    return 0;
}

int cmd_release(Session& session, Args& args) {
    session.client().release(args.exactly(1)[0]);
    return 0;
}

int cmd_hold(Session& session, Args& args) {
    auto words = args.exactly(2);
    session.client().hold(normalize_button(words[0]), ms_argument(words[1]));
    return 0;
}

int cmd_stick(Session& session, Args& args) {
    auto ms = args.option("--ms");
    auto words = args.exactly(3);
    Side side = parse_side(words[0]);
    int x = clamp_stick(parse_i64(words[1]));
    int y = clamp_stick(parse_i64(words[2]));
    if (ms) {
        session.client().tilt(side, x, y, ms_argument(*ms));
    } else {
        session.client().set_stick(side, x, y);
    }
    return 0;
}

int cmd_seq(Session& session, Args& args) {
    bool server = args.flag("--server");
    auto steps = parse_sequence(args.positionals(1, std::numeric_limits<std::size_t>::max(), true));
    if (server) {
        session.client().click_seq(steps);
    } else {
        session.client().sequence(steps);
    }
    return 0;
}

int cmd_cancel(Session& session, Args& args) {
    args.none();
    session.client().click_cancel();
    session.client().touch_cancel();
    return 0;
}

int cmd_controller(Session& session, Args& args) {
    session.client().set_controller_type(parse_controller_type(args.exactly(1)[0]));
    return 0;
}

int cmd_detach(Session& session, Args& args) {
    args.none();
    session.client().detach_controller();
    return 0;
}

// ---- Touch and keyboard ---------------------------------------------------

int cmd_touch(Session& session, Args& args) {
    session.client().touch(point_pairs(args.positionals(2)));
    return 0;
}

int cmd_touch_hold(Session& session, Args& args) {
    auto words = args.exactly(3);
    session.client().touch_hold({parse_int(words[0]), parse_int(words[1])}, ms_argument(words[2]));
    return 0;
}

int cmd_touch_draw(Session& session, Args& args) {
    session.client().touch_draw(point_pairs(args.positionals(4)));
    return 0;
}

int cmd_type(Session& session, Args& args) {
    bool enter = args.flag("--enter");
    auto words = args.positionals(1);
    std::string text;
    for (const auto& word : words) text += (text.empty() ? "" : " ") + word;
    if (enter) text += '\n';
    session.client().type_text(text);
    return 0;
}

std::vector<int> key_codes(const std::vector<std::string>& words) {
    std::vector<int> codes;
    for (const auto& word : words) codes.push_back(key_code(word));
    return codes;
}

int cmd_key(Session& session, Args& args) {
    session.client().key(key_codes(args.positionals(1)));
    return 0;
}

int cmd_key_multi(Session& session, Args& args) {
    session.client().key_multi(key_codes(args.positionals(1)));
    return 0;
}

int cmd_key_mod(Session& session, Args& args) {
    std::vector<std::pair<int, int>> pairs;
    for (const auto& word : args.positionals(1)) {
        auto colon = word.find(':');
        if (colon == std::string::npos) throw UsageError(std::format("'{}' should look like KEY:MODIFIER", word));
        pairs.emplace_back(key_code(word.substr(0, colon)), modifier_bit(word.substr(colon + 1)));
    }
    session.client().key_mod(pairs);
    return 0;
}

// ---- Memory ---------------------------------------------------------------

int cmd_peek(Session& session, Args& args) {
    Region region = region_option(args);
    Format format = format_option(args, Format::Dump);
    auto words = args.exactly(2);
    std::int64_t offset = parse_i64(words[0]);
    Bytes data = session.client().peek(offset, parse_u64(words[1]), region);
    std::cout << show_bytes(data, format, static_cast<std::uint64_t>(offset)) << '\n';
    return 0;
}

int cmd_peek_multi(Session& session, Args& args) {
    Region region = region_option(args);
    Format format = format_option(args, Format::Hex);
    std::vector<PeekRange> ranges;
    for (const auto& word : args.positionals(1)) {
        auto colon = word.find(':');
        if (colon == std::string::npos) throw UsageError(std::format("'{}' should look like OFFSET:SIZE", word));
        ranges.push_back({parse_i64(word.substr(0, colon)), parse_u64(word.substr(colon + 1))});
    }
    auto results = session.client().peek_multi(ranges, region);
    for (std::size_t i = 0; i < results.size(); ++i) {
        std::cout << show_bytes(results[i], format, static_cast<std::uint64_t>(ranges[i].offset)) << '\n';
    }
    return 0;
}

int cmd_poke(Session& session, Args& args) {
    Region region = region_option(args);
    Format format = format_option(args, Format::Hex, false);
    bool verify = args.flag("--verify");
    auto words = args.exactly(2);
    std::int64_t offset = parse_i64(words[0]);
    Bytes data = encode_value(words[1], format);

    session.client().poke(offset, data, region);
    if (!verify) return 0;

    Bytes readback = session.client().peek(offset, data.size(), region);
    if (readback == data) {
        std::cout << "verified\n";
        return 0;
    }
    std::cout << "MISMATCH: " << to_hex(readback) << '\n';
    return 1;
}

int cmd_pointer(Session& session, Args& args) {
    std::int64_t final_offset = parse_i64(args.option("--final").value_or("0"));
    bool relative = args.flag("--relative");
    auto peek_size = args.option("--peek");
    auto poke_data = args.option("--poke");
    Format format = format_option(args, Format::Hex);
    auto chain = offsets(args.positionals(1));

    if (int modes = relative + peek_size.has_value() + poke_data.has_value(); modes > 1) {
        throw UsageError("pick one of --relative, --peek and --poke");
    }

    Client& client = session.client();
    if (peek_size) {
        std::cout << show_bytes(client.pointer_peek(parse_u64(*peek_size), chain, final_offset), format) << '\n';
    } else if (poke_data) {
        client.pointer_poke(parse_hex_bytes(*poke_data), chain, final_offset);
    } else if (relative) {
        std::cout << std::format("{:#x}\n", client.pointer_relative(chain, final_offset));
    } else if (final_offset != 0) {
        std::cout << std::format("{:#x}\n", client.pointer_address(chain, final_offset));
    } else {
        std::cout << std::format("{:#x}\n", client.pointer(chain));
    }
    return 0;
}

int cmd_freeze(Session& session, Args& args) {
    auto words = args.exactly(2);
    session.client().freeze(parse_i64(words[0]), parse_hex_bytes(words[1]));
    return 0;
}

int cmd_unfreeze(Session& session, Args& args) {
    session.client().unfreeze(parse_i64(args.exactly(1)[0]));
    return 0;
}

int cmd_freezes(Session& session, Args& args) {
    std::string action = to_lower(args.exactly(1)[0]);
    Client& client = session.client();
    if (action == "count") {
        std::cout << client.freeze_count() << '\n';
    } else if (action == "clear") {
        client.freeze_clear();
    } else if (action == "pause") {
        client.freeze_pause();
    } else if (action == "unpause") {
        client.freeze_unpause();
    } else {
        throw UsageError("freezes takes count, clear, pause or unpause");
    }
    return 0;
}

// ---- Settings -------------------------------------------------------------

int cmd_config(Session& session, Args& args) {
    auto words = args.positionals(0, 2);
    if (words.empty()) {
        for (const ConfigKey& key : config_keys()) {
            std::cout << std::format("{:<22} default {:<3} {}\n", key.name, key.default_value, key.help);
        }
        return 0;
    }
    if (words.size() != 2) throw UsageError("config takes a NAME and a VALUE");
    session.client().configure(words[0], parse_int(words[1]));
    return 0;
}

int cmd_raw(Session& session, Args& args) {
    auto words = args.positionals(1, std::numeric_limits<std::size_t>::max(), true);
    std::string line;
    for (const auto& word : words) line += (line.empty() ? "" : " ") + word;
    std::string reply = session.client().raw(line);
    if (!reply.empty()) std::cout << reply << '\n';
    return 0;
}

// ---- Scripting ------------------------------------------------------------

int cmd_wait(Session&, Args& args) {
    sleep_ms(ms_argument(args.exactly(1)[0]));
    return 0;
}

std::string join_words(const std::vector<std::string>& words) {
    std::string out;
    for (const auto& word : words) out += (out.empty() ? "" : " ") + word;
    return out;
}

int run_script(Session& session, std::string_view script, bool keep_going) {
    int result = 0;
    for (const auto& words : split_commands(script)) {
        try {
            int code = run_command(session, words, true);
            if (code != 0 && result == 0) result = code;
        } catch (const Interrupted&) {
            throw;
        } catch (const std::exception& error) {
            int code = report_error(error, join_words(words));
            if (!keep_going) {
                session.mark_failed();
                return code;
            }
            session.resync();
            if (result == 0) result = code;
        }
    }
    return result;
}

int cmd_batch(Session& session, Args& args) {
    bool keep_going = args.flag("--keep-going", "-k");
    std::string script;
    while (auto inline_script = args.option("--execute", "-e")) script += *inline_script + "\n";

    auto files = args.positionals(0, 1);
    if (!files.empty() || script.empty()) {
        std::string path = files.empty() ? "-" : files[0];
        if (path == "-") {
            script += std::string(std::istreambuf_iterator<char>(std::cin), {});
        } else {
            std::ifstream file(path);
            if (!file) throw UsageError("cannot read " + path);
            script += std::string(std::istreambuf_iterator<char>(file), {});
        }
    }
    return run_script(session, script, keep_going);
}

int cmd_shell(Session& session, Args& args) {
    args.none();
    const bool interactive = ::isatty(STDIN_FILENO);
    if (interactive) {
        Client& client = session.client();
        std::cout << std::format("connected to {}:{} (sys-botbase {}). Type a command, help, or quit.\n",
                                 client.options().host, client.options().port, client.version());
    }

    std::string line;
    for (;;) {
        if (interactive) std::cout << "botbase> " << std::flush;
        if (!std::getline(std::cin, line)) {
            if (std::cin.eof() || !interrupted()) break;
            // Ctrl-C at the prompt clears the line instead of quitting.
            clear_interrupt();
            std::cin.clear();
            std::cout << '\n';
            continue;
        }

        try {
            for (const auto& words : split_commands(line)) {
                if (words[0] == "quit" || words[0] == "exit") return 0;
                try {
                    run_command(session, words, true);
                } catch (const Interrupted&) {
                    session.recover();
                    std::cerr << "interrupted\n";
                    break;
                } catch (const std::exception& error) {
                    report_error(error);
                    session.resync();
                }
            }
        } catch (const UsageError& error) {
            report_error(error);
        }
    }
    if (interactive) std::cout << '\n';
    return 0;
}

int cmd_help(Session&, Args& args) {
    auto words = args.positionals(0, 1);
    if (words.empty()) {
        print_usage(std::cout);
        return 0;
    }
    if (!print_command_help(words[0], std::cout)) throw UsageError(std::format("no command named '{}'", words[0]));
    return 0;
}

// ---- Table ----------------------------------------------------------------

constexpr std::array kCommands = {
    Command{"discover", "Console", "discover [--subnet CIDR]", "find consoles running sys-botbase", cmd_discover, true},
    Command{"info", "Console", "info [--json]", "version, battery, running game, build ID, memory access", cmd_info},
    Command{"screenshot", "Console", "screenshot [-o FILE|-]", "save the screen as a JPEG (- writes to stdout)", cmd_screenshot},
    Command{"screen", "Console", "screen on|off", "turn the built-in screen on or off", cmd_screen},
    Command{"home", "Console", "home", "go to the HOME menu", cmd_home},
    Command{"close", "Console", "close", "close the running game (HOME, X, A)", cmd_close},
    Command{"running", "Console", "running PROGRAM_ID", "exit 0 if the program is running, 1 if not", cmd_running},
    Command{"game", "Console", "game name|author|version|rating|icon [-o FILE]", "running game metadata", cmd_game},

    Command{"click", "Controller", "click BUTTON... [--gap MS]", "press and release buttons, 150 ms apart", cmd_click},
    Command{"press", "Controller", "press BUTTON", "hold a button down until release", cmd_press},
    Command{"release", "Controller", "release BUTTON", "let go of a button", cmd_release},
    Command{"hold", "Controller", "hold BUTTON MS", "hold a button for MS milliseconds", cmd_hold},
    Command{"stick", "Controller", "stick left|right X Y [--ms MS]", "set a stick (-32768..32767, up is +Y); --ms centres it after", cmd_stick},
    Command{"seq", "Controller", "seq STEP... [--server]", "run inputs in order: A  W250  +A  -A  %X,Y  &X,Y", cmd_seq},
    Command{"cancel", "Controller", "cancel", "stop a running clickSeq and touch", cmd_cancel},
    Command{"controller", "Controller", "controller TYPE", "virtual controller type: pro, joycon-right, or a number", cmd_controller},
    Command{"detach", "Controller", "detach", "remove the virtual controller", cmd_detach},

    Command{"touch", "Touch and keyboard", "touch X Y [X Y...]", "tap points on the 1280x720 screen", cmd_touch},
    Command{"touch-hold", "Touch and keyboard", "touch-hold X Y MS", "hold a finger down", cmd_touch_hold},
    Command{"touch-draw", "Touch and keyboard", "touch-draw X Y X Y [X Y...]", "drag a finger through the points", cmd_touch_draw},
    Command{"type", "Touch and keyboard", "type TEXT... [--enter]", "type into the focused text field (US layout)", cmd_type},
    Command{"key", "Touch and keyboard", "key KEY...", "type keys by name or code: Return, Backspace, A, F5", cmd_key},
    Command{"key-multi", "Touch and keyboard", "key-multi KEY...", "press keys together, e.g. LeftShift A", cmd_key_multi},
    Command{"key-mod", "Touch and keyboard", "key-mod KEY:MODIFIER...", "type keys with modifiers (the system keyboard ignores Shift)", cmd_key_mod},

    Command{"peek", "Memory", "peek OFFSET SIZE [--region heap|main|absolute] [--as FORMAT]", "read game memory", cmd_peek},
    Command{"peek-multi", "Memory", "peek-multi OFFSET:SIZE... [--region R] [--as FORMAT]", "read several ranges in one round trip", cmd_peek_multi},
    Command{"poke", "Memory", "poke OFFSET VALUE [--region R] [--as FORMAT] [--verify]", "write game memory", cmd_poke},
    Command{"pointer", "Memory", "pointer OFFSET... [--final N] [--relative | --peek SIZE | --poke HEX] [--as FORMAT]", "follow a pointer chain from main", cmd_pointer},
    Command{"freeze", "Memory", "freeze OFFSET HEX", "keep rewriting bytes at heap+OFFSET", cmd_freeze},
    Command{"unfreeze", "Memory", "unfreeze OFFSET", "stop freezing heap+OFFSET", cmd_unfreeze},
    Command{"freezes", "Memory", "freezes count|clear|pause|unpause", "manage all freezes", cmd_freezes},

    Command{"config", "Settings", "config [NAME VALUE]", "change a sys-botbase setting, or list them", cmd_config},
    Command{"raw", "Settings", "raw LINE...", "send one sys-botbase command as typed", cmd_raw},

    Command{"wait", "Scripting", "wait MS", "pause between commands", cmd_wait},
    Command{"batch", "Scripting", "batch [FILE|-] [-e SCRIPT]... [--keep-going]", "run many commands over one connection", cmd_batch, true},
    Command{"shell", "Scripting", "shell", "interactive prompt", cmd_shell, true},
    Command{"help", "Scripting", "help [COMMAND]", "show help", cmd_help},
};

const Command* find_command(std::string_view name) {
    auto it = std::ranges::find(kCommands, name, &Command::name);
    return it == kCommands.end() ? nullptr : &*it;
}

}  // namespace

int run_command(Session& session, const std::vector<std::string>& words, bool nested) {
    const Command* command = find_command(words.at(0));
    if (!command) throw UsageError(std::format("no command named '{}'; try botbase help", words[0]));
    if (nested && command->needs_top_level) throw UsageError(std::format("{} cannot run inside a script", command->name));

    Args args(std::string(command->name), std::vector<std::string>(words.begin() + 1, words.end()));
    if (args.flag("--help", "-h")) {
        print_command_help(command->name, std::cout);
        return 0;
    }

    try {
        return command->run(session, args);
    } catch (const UsageError& error) {
        throw UsageError(std::format("{}: {}\nusage: botbase {}", command->name, error.what(), command->usage));
    }
}

void print_usage(std::ostream& out) {
    out << "botbase " BOTBASE_VERSION ": control a Nintendo Switch running sys-botbase\n\n"
           "usage: botbase [OPTIONS] COMMAND [ARGS...]\n\n"
           "options:\n"
           "  --host HOST        console address ($BOTBASE_HOST; default: last found, then a scan)\n"
           "  --port PORT        sys-botbase port ($BOTBASE_PORT; default 6000)\n"
           "  --interface IF     bind to one interface, e.g. en0 under a VPN ($BOTBASE_INTERFACE)\n"
           "  --subnet CIDR      subnet to scan ($BOTBASE_SUBNET; default: the local /24)\n"
           "  --timeout SECONDS  reply timeout (default 10)\n"
           "  --fast             mainLoopSleepTime 0 while running, put back to 50 at exit\n"
           "  --detach           remove the virtual controller at exit\n"
           "  --no-cache         ignore and do not update the remembered console\n"
           "  -q, --quiet        no status messages on stderr\n"
           "  --version          print the version\n";

    std::string_view group;
    for (const Command& command : kCommands) {
        if (command.group != group) {
            group = command.group;
            out << "\n" << group << ":\n";
        }
        out << std::format("  {:<12} {}\n", command.name, command.summary);
    }
    out << "\nRun botbase help COMMAND for its arguments.\n";
}

bool print_command_help(std::string_view name, std::ostream& out) {
    const Command* command = find_command(name);
    if (!command) return false;
    out << "usage: botbase " << command->usage << "\n\n" << command->summary << "\n";
    if (name == "peek" || name == "peek-multi" || name == "poke" || name == "pointer") {
        out << "\nFORMAT is one of: " << format_names() << "\n";
    }
    if (name == "click" || name == "press" || name == "release" || name == "hold" || name == "seq") {
        out << "\nbuttons:";
        for (auto button : button_names()) out << ' ' << button;
        out << "\naliases: up down left right + - start select ls rs l3 r3 screenshot\n";
    }
    if (name == "controller") {
        out << "\ntypes:";
        for (const auto& type : controller_types()) out << ' ' << type.name;
        out << '\n';
    }
    return true;
}

int report_error(const std::exception& error, std::string_view context) {
    std::string where = context.empty() ? "" : std::format(" (in '{}')", context);
    if (const auto* known = dynamic_cast<const Error*>(&error)) {
        std::cerr << std::format("botbase: {}{}: {}\n", known->kind(), where, known->what());
        return static_cast<int>(known->code());
    }
    std::cerr << std::format("botbase: {}{}\n", error.what(), where);
    return static_cast<int>(ExitCode::Failure);
}

}  // namespace botbase::cli
