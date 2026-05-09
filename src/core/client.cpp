#include "botbase/client.hpp"

#include <algorithm>
#include <exception>
#include <format>

#include "botbase/errors.hpp"
#include "botbase/signals.hpp"
#include "socket.hpp"

namespace botbase {

namespace {

using Clock = std::chrono::steady_clock;

// When the debug attach fails, sys-botbase prints whatever svcGetInfo left in
// its output variable. On HOS 22 that is 0x4. No real heap starts this low.
constexpr std::uint64_t kMinPlausibleHeap = 0x100000;

constexpr std::string_view kNoGamePrefix = "nsGetApplicationControlData() failed: ";

constexpr std::string_view kDebuggerHelp =
    "Atmosphere's cheat engine usually holds it: SaltyNX forces dmnt onto every game with a "
    "non-empty atmosphere/contents/<title id>/cheats folder, even with L held at launch. "
    "Rename that folder or disable SaltyNX to give sys-botbase the debugger.";

constexpr int kMaxPointerReads = 40;
constexpr std::size_t kMaxKeysPerCommand = 500;
constexpr int kLeftShift = 225;

std::string_view first_word(std::string_view line) { return line.substr(0, line.find(' ')); }

std::string_view peek_command(Region region) {
    switch (region) {
        case Region::Heap: return "peek";
        case Region::Main: return "peekMain";
        case Region::Absolute: return "peekAbsolute";
    }
    return "peek";
}

std::string_view peek_multi_command(Region region) {
    switch (region) {
        case Region::Heap: return "peekMulti";
        case Region::Main: return "peekMainMulti";
        case Region::Absolute: return "peekAbsoluteMulti";
    }
    return "peekMulti";
}

std::string_view poke_command(Region region) {
    switch (region) {
        case Region::Heap: return "poke";
        case Region::Main: return "pokeMain";
        case Region::Absolute: return "pokeAbsolute";
    }
    return "poke";
}

std::string data_arg(const Bytes& data) {
    if (data.empty()) throw UsageError("data must not be empty");
    return "0x" + to_hex(data, false);
}

std::string chain_args(std::span<const std::int64_t> chain) {
    if (chain.empty()) throw UsageError("a pointer chain needs at least one offset");
    std::string out;
    for (std::int64_t offset : chain) out += " " + hex_arg(offset);
    return out;
}

std::string points_args(std::span<const Point> points) {
    std::string out;
    for (const Point& p : points) {
        if (p.x < 0 || p.x > kScreenWidth || p.y < 0 || p.y > kScreenHeight) {
            throw UsageError(std::format("touch point ({}, {}) is outside the {}x{} screen", p.x, p.y, kScreenWidth, kScreenHeight));
        }
        out += std::format(" {} {}", p.x, p.y);
    }
    return out;
}

Bytes memory_reply(const std::string& reply, std::string_view command, std::uint64_t expected_size) {
    if (reply.empty()) {
        throw MemoryAccessError(std::format("{} failed: the address is unmapped, no game is running, or the debugger is busy", command));
    }
    auto bytes = decode_hex(reply);
    if (!bytes) throw ProtocolError(std::format("{} sent a reply that is not hex", command));
    if (bytes->size() != expected_size) {
        throw ProtocolError(std::format("{} sent {} bytes instead of {}", command, bytes->size(), expected_size));
    }
    return std::move(*bytes);
}

std::vector<Bytes> split_bytes(const Bytes& data, std::span<const std::uint64_t> sizes) {
    std::vector<Bytes> parts;
    auto it = data.begin();
    for (std::uint64_t size : sizes) {
        parts.emplace_back(it, it + static_cast<std::ptrdiff_t>(size));
        it += static_cast<std::ptrdiff_t>(size);
    }
    return parts;
}

std::uint64_t address_reply(const std::string& reply, std::string_view command) {
    if (reply.empty()) throw MemoryAccessError(std::format("{}: the pointer chain could not be followed", command));
    auto value = parse_hex_number(reply);
    if (!value) throw ProtocolError(std::format("{} sent '{}' instead of an address", command, reply));
    return *value;
}

int integer_reply(const std::string& reply, std::string_view command) {
    try {
        return parse_int(reply);
    } catch (const UsageError&) {
        throw ProtocolError(std::format("{} sent '{}' instead of a number", command, reply));
    }
}

}  // namespace

Client::Client(ConnectOptions options)
    : options_(std::move(options)),
      socket_(std::make_unique<detail::Socket>(
          detail::Socket::connect(options_.host, options_.port, options_.interface, options_.timeout))) {
    for (const ConfigKey& key : config_keys()) settings_.emplace(key.name, key.default_value);
}

Client::~Client() = default;

// ---- Transport ------------------------------------------------------------

double Client::timeout_or_default(double timeout) const { return timeout < 0 ? options_.timeout : timeout; }

void Client::send(std::string_view data) { socket_->send_all(data, options_.timeout); }

std::string Client::read_line(double timeout) {
    const double wait = timeout_or_default(timeout);
    for (;;) {
        std::size_t newline;
        while ((newline = buffer_.find('\n', scanned_)) == std::string::npos) {
            scanned_ = buffer_.size();
            char chunk[1 << 16];
            std::size_t got = socket_->receive(chunk, sizeof chunk, wait);
            if (got == 0) throw ConnectionError("sys-botbase closed the connection");
            buffer_.append(chunk, got);
        }

        std::string line = buffer_.substr(0, newline);
        buffer_.erase(0, newline + 1);
        scanned_ = 0;
        if (!line.empty() && line.back() == '\r') line.pop_back();

        // A finished clickSeq prints "done" into whichever socket spoke last,
        // at any moment. It is never the answer to anything we asked.
        if (line == "done") {
            ++click_seq_done_;
            click_seq_running_ = false;
            continue;
        }
        if (pending_replies_ > 0) --pending_replies_;
        return line;
    }
}

std::string Client::transact(std::string_view line, std::string_view word, double timeout) {
    if (line.find_first_of("\r\n") != std::string_view::npos) throw UsageError("a command must be a single line");
    if (line.size() + 2 > kMaxLineLength) {
        throw UsageError(std::format("command is {} characters; sys-botbase reads at most {}", line.size(), kMaxLineLength - 2));
    }

    const bool replies = is_reply_command(word);
    std::string wire = std::string(line) + "\r\n";
    if (!replies) wire += "getVersion\r\n";

    send(wire);
    ++pending_replies_;
    std::string reply = read_line(timeout);
    if (replies) return reply;

    if (!is_version_string(reply)) {
        throw ProtocolError(std::format("expected the getVersion fence after '{}', got '{}'", word, reply.substr(0, 80)));
    }
    return {};
}

std::string Client::command(std::string_view line, double timeout) {
    std::string_view word = first_word(line);
    if (!is_known_command(word)) throw UsageError(std::format("unknown sys-botbase command '{}'", word));
    return transact(line, word, timeout);
}

std::string Client::raw(std::string_view line, double timeout) { return transact(line, first_word(line), timeout); }

std::string Client::sync(double timeout) {
    send("getVersion\r\n");
    ++pending_replies_;
    return read_line(timeout);
}

void Client::resync() {
    const double wait = std::max(options_.timeout, 30.0);
    while (pending_replies_ > 0) read_line(wait);
}

void Client::reconnect() {
    *socket_ = detail::Socket::connect(options_.host, options_.port, options_.interface, options_.timeout);
    buffer_.clear();
    scanned_ = 0;
    pending_replies_ = 0;
}

// ---- System ---------------------------------------------------------------

std::string Client::version() { return command("getVersion"); }

int Client::charge() { return integer_reply(command("charge"), "charge"); }

std::pair<int, std::string> Client::system_language() {
    int code = integer_reply(command("getSystemLanguage"), "getSystemLanguage");
    return {code, language_name(code)};
}

bool Client::is_program_running(std::uint64_t program_id) {
    return command("isProgramRunning " + hex_arg_unsigned(program_id)) == "1";
}

int Client::fd_count() { return integer_reply(command("fdCount"), "fdCount"); }

// ---- Running game ---------------------------------------------------------

namespace {

std::optional<std::uint64_t> optional_hex(const std::string& reply, std::string_view command) {
    if (reply.empty()) return std::nullopt;
    auto value = parse_hex_number(reply);
    if (!value) throw ProtocolError(std::format("{} sent '{}'", command, reply));
    return value;
}

}  // namespace

std::optional<std::uint64_t> Client::title_id() { return optional_hex(command("getTitleID"), "getTitleID"); }

std::optional<std::uint64_t> Client::title_version() {
    return optional_hex(command("getTitleVersion"), "getTitleVersion");
}

std::optional<std::string> Client::build_id() {
    std::string reply = command("getBuildID");
    if (reply.empty()) return std::nullopt;
    return reply;
}

std::optional<std::uint64_t> Client::main_nso_base() {
    return optional_hex(command("getMainNsoBase"), "getMainNsoBase");
}

std::optional<std::uint64_t> Client::heap_base() {
    auto base = optional_hex(command("getHeapBase"), "getHeapBase");
    if (base && *base < kMinPlausibleHeap) {
        throw DebuggerBusy(std::format("getHeapBase returned {:#x}, so another debugger holds the game. {}", *base, kDebuggerHelp));
    }
    return base;
}

std::uint64_t Client::require_memory_access() {
    auto base = heap_base();
    if (!base) throw NoGameRunning("no game is running");
    return *base;
}

std::string Client::game_field(std::string_view field) {
    if (field != "name" && field != "author" && field != "version" && field != "rating") {
        throw UsageError(std::format("game field must be name, author, version or rating, not '{}'", field));
    }
    std::string reply = command(std::format("game {}", field));
    if (reply.starts_with(kNoGamePrefix)) throw NoGameRunning("no game is running (" + reply + ")");
    return reply;
}

Bytes Client::game_icon() {
    std::string reply = command("game icon", std::max(options_.timeout, 20.0));
    if (reply.starts_with(kNoGamePrefix)) throw NoGameRunning("no game is running (" + reply + ")");
    auto icon = decode_hex(reply);
    if (!icon) throw ProtocolError("game icon sent a reply that is not hex");
    return std::move(*icon);
}

ConsoleInfo Client::info() {
    ConsoleInfo info;
    info.version = version();
    info.charge = charge();
    info.language = system_language().second;

    auto tid = title_id();
    if (!tid) return info;

    GameInfo game;
    game.title_id = *tid;
    game.build_id = build_id().value_or("");
    game.title_version = title_version().value_or(0);
    game.main_nso_base = main_nso_base().value_or(0);
    try {
        game.heap_base = heap_base();
    } catch (const DebuggerBusy&) {
    }
    try {
        game.name = game_field("name");
    } catch (const NoGameRunning&) {
    }
    info.game = std::move(game);
    return info;
}

// ---- Memory ---------------------------------------------------------------

Bytes Client::peek(std::int64_t offset, std::uint64_t size, Region region) {
    if (size == 0) throw UsageError("peek size must be at least 1");
    std::string_view name = peek_command(region);
    double timeout = std::max(options_.timeout, static_cast<double>(size) / 50000.0);
    std::string reply = command(std::format("{} {} {}", name, hex_arg(offset), hex_arg_unsigned(size)), timeout);
    return memory_reply(reply, name, size);
}

std::vector<Bytes> Client::peek_multi(std::span<const PeekRange> ranges, Region region) {
    if (ranges.empty()) return {};
    std::string_view name = peek_multi_command(region);
    std::string line(name);
    std::vector<std::uint64_t> sizes;
    std::uint64_t total = 0;
    for (const PeekRange& range : ranges) {
        if (range.size == 0) throw UsageError("peek size must be at least 1");
        line += std::format(" {} {}", hex_arg(range.offset), hex_arg_unsigned(range.size));
        sizes.push_back(range.size);
        total += range.size;
    }
    return split_bytes(memory_reply(command(line), name, total), sizes);
}

void Client::poke(std::int64_t offset, const Bytes& data, Region region) {
    command(std::format("{} {} {}", poke_command(region), hex_arg(offset), data_arg(data)));
}

std::uint64_t Client::pointer(std::span<const std::int64_t> chain) {
    return address_reply(command("pointer" + chain_args(chain)), "pointer");
}

std::uint64_t Client::pointer_address(std::span<const std::int64_t> chain, std::int64_t final_offset) {
    return address_reply(command("pointerAll" + chain_args(chain) + " " + hex_arg(final_offset)), "pointerAll");
}

std::uint64_t Client::pointer_relative(std::span<const std::int64_t> chain, std::int64_t final_offset) {
    return address_reply(command("pointerRelative" + chain_args(chain) + " " + hex_arg(final_offset)), "pointerRelative");
}

Bytes Client::pointer_peek(std::uint64_t size, std::span<const std::int64_t> chain, std::int64_t final_offset) {
    if (size == 0) throw UsageError("peek size must be at least 1");
    std::string line = std::format("pointerPeek {}{} {}", hex_arg_unsigned(size), chain_args(chain), hex_arg(final_offset));
    return memory_reply(command(line), "pointerPeek", size);
}

std::vector<Bytes> Client::pointer_peek_multi(std::span<const PointerRead> reads) {
    if (reads.empty() || reads.size() > kMaxPointerReads) {
        throw UsageError(std::format("pointerPeekMulti takes 1 to {} chains", kMaxPointerReads));
    }
    std::string line = "pointerPeekMulti";
    std::vector<std::uint64_t> sizes;
    std::uint64_t total = 0;
    for (std::size_t i = 0; i < reads.size(); ++i) {
        const PointerRead& read = reads[i];
        if (i > 0) line += " *";
        line += std::format(" {}{} {}", hex_arg_unsigned(read.size), chain_args(read.chain), hex_arg(read.final_offset));
        sizes.push_back(read.size);
        total += read.size;
    }
    return split_bytes(memory_reply(command(line), "pointerPeekMulti", total), sizes);
}

void Client::pointer_poke(const Bytes& data, std::span<const std::int64_t> chain, std::int64_t final_offset) {
    command(std::format("pointerPoke {}{} {}", data_arg(data), chain_args(chain), hex_arg(final_offset)));
}

// ---- Freezes --------------------------------------------------------------

void Client::freeze(std::int64_t heap_offset, const Bytes& data) {
    command(std::format("freeze {} {}", hex_arg(heap_offset), data_arg(data)));
}

void Client::unfreeze(std::int64_t heap_offset) { command("unFreeze " + hex_arg(heap_offset)); }

int Client::freeze_count() {
    std::string reply = command("freezeCount");
    auto count = parse_hex_number(reply);
    if (!count) throw ProtocolError(std::format("freezeCount sent '{}'", reply));
    return static_cast<int>(*count);
}

void Client::freeze_clear() { command("freezeClear"); }
void Client::freeze_pause() { command("freezePause"); }
void Client::freeze_unpause() { command("freezeUnpause"); }

// ---- Controller -----------------------------------------------------------

void Client::click(std::string_view button) { command("click " + normalize_button(button)); }

void Client::press(std::string_view button) {
    std::string name = normalize_button(button);
    command("press " + name);
    held_buttons_.insert(name);
}

void Client::release(std::string_view button) {
    std::string name = normalize_button(button);
    command("release " + name);
    held_buttons_.erase(name);
}

void Client::hold(std::string_view button, int ms) {
    press(button);
    sleep_ms(ms);
    release(button);
}

void Client::set_stick(Side side, int x, int y) {
    x = clamp_stick(x);
    y = clamp_stick(y);
    command(std::format("setStick {} {} {}", side_name(side), hex_arg(x), hex_arg(y)));
    stick_moved_[side == Side::Left ? 0 : 1] = x != 0 || y != 0;
}

void Client::tilt(Side side, int x, int y, int ms) {
    set_stick(side, x, y);
    sleep_ms(ms);
    set_stick(side, 0, 0);
}

void Client::detach_controller() {
    command("detachController");
    held_buttons_.clear();
    stick_moved_[0] = stick_moved_[1] = false;
}

void Client::set_controller_type(int type) { configure("controllerType", type); }

void Client::sequence(std::span<const Step> steps) {
    for (const Step& step : steps) {
        switch (step.kind) {
            case Step::Kind::Click: click(step.button); break;
            case Step::Kind::Wait: sleep_ms(step.ms); break;
            case Step::Kind::Press: press(step.button); break;
            case Step::Kind::Release: release(step.button); break;
            case Step::Kind::Stick: set_stick(step.side, step.x, step.y); break;
            case Step::Kind::Hold: hold(step.button, step.ms); break;
        }
    }
}

void Client::click_seq(std::span<const Step> steps, bool wait, double timeout) {
    std::string text = build_click_seq(steps);
    click_seq_steps_.assign(steps.begin(), steps.end());
    const int finished_before = click_seq_done_;

    click_seq_running_ = true;
    command("clickSeq " + text);
    if (!wait) return;

    const auto deadline = Clock::now() + std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(timeout));
    while (click_seq_done_ == finished_before) {
        if (Clock::now() > deadline) throw TimeoutError(std::format("clickSeq did not finish within {:g} s", timeout));
        sync();
        sleep_ms(100);
    }
}

void Client::click_cancel() { command("clickCancel"); }

// ---- Touch ----------------------------------------------------------------

void Client::touch(std::span<const Point> points, bool wait) {
    if (points.empty()) throw UsageError("touch needs at least one point");
    command("touch" + points_args(points));
    sleep_for_worker(static_cast<double>(points.size() * 2) * setting("pollRate") + 50, wait);
}

void Client::touch_hold(Point point, int ms, bool wait) {
    ms = std::max(ms, 15);
    command(std::format("touchHold{} {}", points_args(std::span(&point, 1)), ms));
    sleep_for_worker(setting("pollRate") + ms + 50, wait);
}

void Client::touch_draw(std::span<const Point> points, bool wait) {
    if (points.size() < 2) throw UsageError("touch-draw needs at least two points");
    command("touchDraw" + points_args(points));
    sleep_for_worker(static_cast<double>(points.size() * 2 + 1) * setting("pollRate") + 50, wait);
}

void Client::touch_cancel() {
    command("touchCancel");
    touch_busy_until_ = {};
}

void Client::sleep_for_worker(double ms, bool wait) {
    touch_busy_until_ = Clock::now() + std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double, std::milli>(ms));
    if (wait) sleep_ms(ms);
}

// ---- Keyboard -------------------------------------------------------------

namespace {

std::string codes_args(std::span<const int> codes) {
    std::string out;
    for (int code : codes) out += std::format(" {}", key_code(code));
    return out;
}

}  // namespace

void Client::key(std::span<const int> codes, bool wait) {
    if (codes.empty()) throw UsageError("key needs at least one key");
    command("key" + codes_args(codes));
    if (wait) sleep_ms(static_cast<double>(codes.size()) * (setting("keySleepTime") + setting("pollRate")) + 50);
}

void Client::key_mod(std::span<const std::pair<int, int>> code_and_modifier_bit, bool wait) {
    if (code_and_modifier_bit.empty()) throw UsageError("key-mod needs at least one key");
    std::string line = "keyMod";
    for (const auto& [code, bit] : code_and_modifier_bit) line += std::format(" {} {}", key_code(code), bit);
    command(line);
    if (wait) {
        sleep_ms(static_cast<double>(code_and_modifier_bit.size()) * (setting("keySleepTime") + setting("pollRate")) + 50);
    }
}

void Client::key_multi(std::span<const int> codes, bool wait) {
    if (codes.empty()) throw UsageError("key-multi needs at least one key");
    command("keyMulti" + codes_args(codes));
    if (wait) sleep_ms(setting("keySleepTime") + setting("pollRate") + 50);
}

void Client::type_text(std::string_view text, bool wait) {
    std::string missing;
    for (char c : text) {
        if (!us_layout(c) && missing.find(c) == std::string::npos) missing += c;
    }
    if (!missing.empty()) throw UsageError(std::format("cannot type {} on a US keyboard layout", json_quote(missing)));

    std::vector<int> run;
    auto flush = [&] {
        if (!run.empty()) key(run, wait);
        run.clear();
    };
    for (char c : text) {
        KeyStroke stroke = *us_layout(c);
        if (stroke.shift) {
            flush();
            const int chord[] = {kLeftShift, stroke.code};
            key_multi(chord, wait);
        } else {
            run.push_back(stroke.code);
            if (run.size() >= kMaxKeysPerCommand) flush();
        }
    }
    flush();
}

// ---- Screen ---------------------------------------------------------------

Bytes Client::screenshot() {
    std::string reply = command("pixelPeek", std::max(options_.timeout, 30.0));
    auto jpeg = decode_hex(reply);
    if (!jpeg || jpeg->size() < 3 || (*jpeg)[0] != 0xFF || (*jpeg)[1] != 0xD8 || (*jpeg)[2] != 0xFF) {
        throw ProtocolError("pixelPeek did not return a JPEG");
    }
    return std::move(*jpeg);
}

void Client::screen_on() { command("screenOn"); }
void Client::screen_off() { command("screenOff"); }

// ---- Settings -------------------------------------------------------------

void Client::configure(std::string_view name, int value) {
    if (!find_config_key(name)) {
        std::string valid;
        for (const ConfigKey& key : config_keys()) valid += std::format("{}{}", valid.empty() ? "" : ", ", key.name);
        throw UsageError(std::format("unknown setting '{}'; valid: {}", name, valid));
    }
    if ((name == "echoCommands" || name == "printDebugResultCodes") && value != 0) {
        throw UsageError(std::format(
            "{} {} adds lines to every reply, for every client, and breaks reply framing. "
            "Send it with raw if you really want it.", name, value));
    }

    command(std::format("configure {} {}", name, value));
    settings_.insert_or_assign(std::string(name), value);
    if (name == "controllerType") {
        held_buttons_.clear();
        stick_moved_[0] = stick_moved_[1] = false;
    }
}

void Client::fast_mode(bool enabled) { configure("mainLoopSleepTime", enabled ? 0 : 50); }

int Client::setting(std::string_view name) const {
    auto it = settings_.find(name);
    return it == settings_.end() ? 0 : it->second;
}

// ---- HOME menu ------------------------------------------------------------

void Client::go_home() {
    click("HOME");
    sleep_ms(1000);
}

bool Client::close_app(double settle_seconds) {
    auto tid = title_id();
    if (!tid) return true;
    go_home();
    click("X");
    sleep_ms(settle_seconds * 1000);
    click("A");
    sleep_ms(settle_seconds * 2000);
    return !is_program_running(*tid);
}

// ---- Safety ---------------------------------------------------------------

void Client::release_inputs() {
    std::exception_ptr first_error;
    auto attempt = [&](auto&& action) {
        try {
            action();
        } catch (...) {
            if (!first_error) first_error = std::current_exception();
        }
    };

    if (click_seq_running_) {
        attempt([&] { click_cancel(); });
        for (const Step& step : click_seq_steps_) {
            if (step.kind == Step::Kind::Press) held_buttons_.insert(step.button);
            if (step.kind == Step::Kind::Stick) stick_moved_[step.side == Side::Left ? 0 : 1] = true;
        }
        click_seq_running_ = false;
    }
    if (Clock::now() < touch_busy_until_) attempt([&] { touch_cancel(); });

    for (const std::string& button : std::set(held_buttons_)) attempt([&] { release(button); });
    if (stick_moved_[0]) attempt([&] { set_stick(Side::Left, 0, 0); });
    if (stick_moved_[1]) attempt([&] { set_stick(Side::Right, 0, 0); });

    if (first_error) std::rethrow_exception(first_error);
}

bool Client::holding_inputs() const {
    return !held_buttons_.empty() || stick_moved_[0] || stick_moved_[1] || click_seq_running_ ||
           Clock::now() < touch_busy_until_;
}

}  // namespace botbase
