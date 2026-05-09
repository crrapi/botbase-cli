#pragma once

#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "botbase/input.hpp"
#include "botbase/protocol.hpp"
#include "botbase/util.hpp"

namespace botbase {

namespace detail {
class Socket;
}

struct ConnectOptions {
    std::string host{};
    std::uint16_t port = kDefaultPort;
    // Bind to this network interface, e.g. "en0" when a VPN routes the LAN subnet.
    std::string interface{};
    double timeout = 10.0;
};

struct GameInfo {
    std::uint64_t title_id = 0;
    std::string build_id;
    std::uint64_t title_version = 0;
    std::uint64_t main_nso_base = 0;
    std::optional<std::uint64_t> heap_base;  // empty when the debugger is busy
    std::optional<std::string> name;
};

struct ConsoleInfo {
    std::string version;
    int charge = 0;
    std::string language;
    std::optional<GameInfo> game;
};

struct PeekRange {
    std::int64_t offset;
    std::uint64_t size;
};

struct PointerRead {
    std::uint64_t size;
    std::vector<std::int64_t> chain;
    std::int64_t final_offset = 0;
};

// One connection to sys-botbase.
//
// Write commands get no reply from sys-botbase, so each one is followed by a
// getVersion fence and the call returns only after the console ran it. Touch,
// keyboard and clickSeq commands run on worker threads over there; their
// methods take `wait` to sleep for roughly as long as the input takes.
//
// Pointer chains start at the main NSO base: [[main + c0] + c1] ... + final.
class Client {
public:
    explicit Client(ConnectOptions options);
    ~Client();

    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;

    const ConnectOptions& options() const { return options_; }

    // Transport
    std::string command(std::string_view line, double timeout = -1);
    std::string raw(std::string_view line, double timeout = -1);
    std::string sync(double timeout = -1);
    void reconnect();
    // Reads and drops replies that an interrupted call left on the wire.
    void resync();

    // System
    std::string version();
    int charge();
    std::pair<int, std::string> system_language();
    bool is_program_running(std::uint64_t program_id);
    int fd_count();

    // Running game. Empty when nothing is in the foreground.
    std::optional<std::uint64_t> title_id();
    std::optional<std::uint64_t> title_version();
    std::optional<std::string> build_id();
    std::optional<std::uint64_t> main_nso_base();
    // Throws DebuggerBusy when another debugger holds the game.
    std::optional<std::uint64_t> heap_base();
    // Heap base, or NoGameRunning / DebuggerBusy. Call before heap work.
    std::uint64_t require_memory_access();
    std::string game_field(std::string_view field);  // name, author, version, rating
    Bytes game_icon();
    ConsoleInfo info();

    // Memory
    Bytes peek(std::int64_t offset, std::uint64_t size, Region region = Region::Heap);
    std::vector<Bytes> peek_multi(std::span<const PeekRange> ranges, Region region = Region::Heap);
    // sys-botbase reports nothing back, even when the write fails.
    void poke(std::int64_t offset, const Bytes& data, Region region = Region::Heap);

    std::uint64_t pointer(std::span<const std::int64_t> chain);
    std::uint64_t pointer_address(std::span<const std::int64_t> chain, std::int64_t final_offset = 0);
    std::uint64_t pointer_relative(std::span<const std::int64_t> chain, std::int64_t final_offset = 0);
    Bytes pointer_peek(std::uint64_t size, std::span<const std::int64_t> chain,
                       std::int64_t final_offset = 0);
    std::vector<Bytes> pointer_peek_multi(std::span<const PointerRead> reads);
    // Not validated upstream: a broken chain writes near `final_offset`.
    void pointer_poke(const Bytes& data, std::span<const std::int64_t> chain,
                      std::int64_t final_offset = 0);

    // Freezes rewrite heap+offset every freezeRate ms. Upstream docs call the
    // address absolute, but the code adds the heap base.
    void freeze(std::int64_t heap_offset, const Bytes& data);
    void unfreeze(std::int64_t heap_offset);
    int freeze_count();
    void freeze_clear();
    void freeze_pause();
    void freeze_unpause();

    // Controller
    void click(std::string_view button);
    void press(std::string_view button);
    void release(std::string_view button);
    void hold(std::string_view button, int ms);
    void set_stick(Side side, int x, int y);
    void tilt(Side side, int x, int y, int ms);
    void detach_controller();
    void set_controller_type(int type);
    // Runs steps one command at a time from this side. Slower than clickSeq,
    // but immune to its strtok bug.
    void sequence(std::span<const Step> steps);
    void click_seq(std::span<const Step> steps, bool wait = true, double timeout = 120);
    void click_cancel();

    // Touch, in screen pixels (1280x720). Works docked too.
    void touch(std::span<const Point> points, bool wait = true);
    void touch_hold(Point point, int ms, bool wait = true);
    void touch_draw(std::span<const Point> points, bool wait = true);
    void touch_cancel();

    // Keyboard
    void key(std::span<const int> codes, bool wait = true);
    void key_mod(std::span<const std::pair<int, int>> code_and_modifier_bit, bool wait = true);
    void key_multi(std::span<const int> codes, bool wait = true);
    // Types on a US layout. Capitals go out as LeftShift chords, because the
    // system keyboard ignores keyMod's Shift.
    void type_text(std::string_view text, bool wait = true);

    // Screen
    Bytes screenshot();
    void screen_on();
    void screen_off();

    // Settings are global inside sys-botbase and last until reboot.
    void configure(std::string_view name, int value);
    void fast_mode(bool enabled = true);

    // HOME menu
    void go_home();
    // HOME, X, A. Returns whether the game is gone afterwards.
    bool close_app(double settle_seconds = 1.2);

    // Lets go of everything this client is holding: buttons, sticks, a
    // running clickSeq and an unfinished touch.
    void release_inputs();
    bool holding_inputs() const;

    int click_seq_done() const { return click_seq_done_; }

private:
    std::string transact(std::string_view line, std::string_view word, double timeout);
    std::string read_line(double timeout);
    void send(std::string_view data);
    double timeout_or_default(double timeout) const;
    int setting(std::string_view name) const;
    void sleep_for_worker(double ms, bool wait);

    ConnectOptions options_;
    std::unique_ptr<detail::Socket> socket_;
    std::string buffer_;
    std::size_t scanned_ = 0;
    int pending_replies_ = 0;
    int click_seq_done_ = 0;

    std::map<std::string, int, std::less<>> settings_;
    std::set<std::string> held_buttons_;
    bool stick_moved_[2] = {false, false};
    bool click_seq_running_ = false;
    std::vector<Step> click_seq_steps_;
    std::chrono::steady_clock::time_point touch_busy_until_{};
};

}  // namespace botbase
