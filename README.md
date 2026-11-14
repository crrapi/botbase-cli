# botbase-cli

[![CI](https://github.com/crrapi/botbase-cli/actions/workflows/ci.yml/badge.svg)](https://github.com/crrapi/botbase-cli/actions/workflows/ci.yml)

A command-line client and C++20 library for
[sys-botbase](https://github.com/olliz0r/sys-botbase), the Nintendo Switch sysmodule that lets a
computer press buttons, touch the screen, type, take screenshots, and read and write game memory
over Wi-Fi.

It covers every sys-botbase 2.6 command, works around the protocol's traps on the client side, and
puts the console back the way it found it when something goes wrong. One binary, no
dependencies.

```console
$ botbase discover
192.168.1.50    sys-botbase 2.6
$ botbase info
version        2.6
charge         100
language       American English
title_id       01006A800016E000
build_id       21450c647b8c5940
...
$ botbase screenshot -o now.jpg
$ botbase click HOME
$ botbase type "Hello, world!" --enter
$ botbase peek 0x8 4 --region main --as hex
```

> The original Python client lives on the
> [`legacy-python`](https://github.com/crrapi/botbase-cli/tree/legacy-python) branch.

## How it talks to the console

sys-botbase listens on TCP port 6000 for plain text, one command per line:
`click A\r\n`, `peek 0x1000 0x10\r\n`, `pixelPeek\r\n`. Read commands answer with one line, mostly
hex. Write commands answer with nothing at all. There is no handshake and no authentication.

That silence is the hard part. This client follows every write with a `getVersion` and waits for
the reply, so a command has finished on the console by the time `botbase` returns. It also counts
replies still in flight, so an interrupted command never leaves a stray answer for the next one.

## Install

Grab a binary from [Releases](https://github.com/crrapi/botbase-cli/releases): static builds for
Linux x86_64 and arm64, and a universal build for macOS 13.3 and later.

```sh
tar xzf botbase-*.tar.gz
sudo cp botbase-*/botbase /usr/local/bin/
```

The macOS build is not notarized, so Gatekeeper blocks the first run. Clear the quarantine flag
with `xattr -d com.apple.quarantine /usr/local/bin/botbase`, or build from source.

## Build

Needs CMake 3.20+ and a C++20 compiler (Apple Clang 15+, GCC 13+, Clang 17+).

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build      # unit tests, plus CLI tests if python3 is around
cmake --install build       # optional: puts botbase in /usr/local/bin
```

The console needs Atmosphere and sys-botbase. On HOS 21 and later, read
[docs/server-setup.md](docs/server-setup.md) first: the official v2.5 can crash the console at
boot (2001-0132), and that page has the fix.

## Usage

`botbase [OPTIONS] COMMAND [ARGS...]`. Run `botbase help` for everything, or
`botbase help COMMAND` for one command.

| Task | Command |
|---|---|
| Find consoles | `discover [--subnet 192.168.1.0/24]` |
| Status and running game | `info [--json]` |
| Screenshot | `screenshot [-o file.jpg]`, or `-o -` for stdout |
| Buttons | `click A B --gap 200`, `press ZR`, `release ZR`, `hold A 1500` |
| Sticks | `stick left 0 32767 --ms 400` |
| Input sequence | `seq A W500 DDOWN W200 +L %0,32767 W300 -L` (add `--server` for clickSeq) |
| Touch | `touch 640 360`, `touch-hold 100 600 1000`, `touch-draw 100 100 600 100` |
| Keyboard | `type "text" [--enter]`, `key Return`, `key-multi LeftShift A` |
| Read memory | `peek 0x40000 64 [--region heap/main/absolute] [--as dump/hex/u32/f32]` |
| Write memory | `poke 0x40000 99 --as u32 --verify` |
| Pointer chain | `pointer 0x1c50c00 0x10 --final 0x8 --peek 4 --as u32` |
| Freeze a value | `freeze 0x40000 63`, `unfreeze 0x40000`, `freezes count/clear/pause/unpause` |
| Game metadata | `game name`, `game icon -o icon.jpg` |
| Is it running? | `running 0x010000000000100D` (exit code 0 or 1) |
| Settings | `config` lists them, `config mainLoopSleepTime 0`, `controller joycon-right` |
| HOME menu | `home`, `close` |
| Many commands, one connection | `batch -e "click HOME; wait 1000; screenshot -o home.jpg"` |
| Anything else | `raw getTitleVersion`, or `shell` for a prompt |

### Finding the console

`--host` wins, then `$BOTBASE_HOST`. Without either, botbase tries the console it found last time
(kept in `~/.cache/botbase/host`) and scans the local /24 only if that one stopped answering.
`--interface en0` (or `$BOTBASE_INTERFACE`) pins every socket to one network interface, for when
a VPN routes the same subnet as your LAN.

### Global options

| Option | Effect |
|---|---|
| `--fast` | sets `mainLoopSleepTime 0` for the run, about twice as fast, and puts it back to 50 at exit |
| `--detach` | removes the virtual controller at exit so it stops holding a player slot |
| `--timeout S` | reply timeout in seconds (default 10) |
| `--no-cache` | skip the remembered console |
| `-q` | no status messages on stderr |

### Scripts

`batch` runs commands from a file, from stdin, or from `-e`, over a single connection. Commands
are split on newlines and `;`, with shell-style quotes and `#` comments. `wait MS` pauses.
A script stops at its first error unless you pass `--keep-going`.

```sh
botbase batch <<'EOF'
home
wait 1000
touch 910 545    # System Settings
click A
wait 1500
screenshot -o settings.jpg
EOF
```

## What it fixes

- **Every command finishes before botbase returns,** thanks to the `getVersion` fence.
- **Typos never press A.** sys-botbase presses A for any button name it does not know. botbase
  checks names before anything is sent.
- **Ctrl-C lets go.** An interrupted `hold`, `stick --ms`, `seq` or touch releases its buttons,
  centres its sticks and cancels its worker before exiting with code 130. A failed script does the
  same.
- **Busy debuggers are named.** When Atmosphere's cheat engine holds the game (SaltyNX forces this
  for any game with a cheats folder), reads quietly fail. botbase says so, and says how to fix it.
- **`clickSeq` noise is handled.** Its stray `done` lines never leak into other replies, and `seq`
  runs steps from the client by default to dodge an upstream `strtok` bug.
- **Real typing.** `type` maps a US layout and sends capitals as Shift chords, because the system
  keyboard ignores the Shift modifier.
- **Settings that break framing are refused.** `echoCommands` and `printDebugResultCodes` add lines
  to every reply for every client. They stay available through `raw`.

### Exit codes

| Code | Meaning |
|---|---|
| 0 | success |
| 1 | general failure, or a "no" from `running`, `close` or `poke --verify` |
| 2 | bad arguments; nothing was sent |
| 3 | could not connect, or no console found |
| 4 | timed out |
| 5 | no game is running |
| 6 | another debugger holds the game |
| 7 | memory read failed |
| 130 | interrupted |

## Library

Link against `botbase::core` and include `<botbase/botbase.hpp>`.

```cpp
#include <botbase/botbase.hpp>

botbase::Client console({.host = "192.168.1.50", .interface = "en0"});

console.click("HOME");
console.type_text("Player 1");
botbase::Bytes jpeg = console.screenshot();

try {
    console.require_memory_access();
    botbase::Bytes hp = console.peek(0x40100, 4);
    console.poke(0x40100, {0xE7, 0x03, 0x00, 0x00});
} catch (const botbase::DebuggerBusy& error) {
    std::cerr << error.what() << '\n';  // explains the cheat-engine conflict
}
console.detach_controller();
```

See [examples/](examples) and [include/botbase/client.hpp](include/botbase/client.hpp).
[docs/commands.md](docs/commands.md) maps each wire command to its method and CLI form, with
hardware test status.

## Read before you use it

- **Port 6000 has no authentication.** Anyone who reaches it can control the console and edit
  game memory. Keep it on a trusted LAN.
- **Writes are immediate.** Buttons press a real console; pokes change a running game. Memory
  writes in an online game are a ban risk.
- **Settings are global** and last until reboot.
- [docs/quirks.md](docs/quirks.md) lists every trap found while testing.

## Layout

```text
include/botbase/   public headers for the library
src/core/          protocol, sockets, input mapping, discovery
src/cli/           the botbase command: argument parsing, commands, session cleanup
tests/unit/        library tests that need no console
tests/cli/         the real binary against fake_server.py, a stand-in for sys-botbase
examples/          small programs using the library
docs/              command reference, quirks, server setup
```

## Credits

[olliz0r/sys-botbase](https://github.com/olliz0r/sys-botbase) and its contributors; the HOS 22
memory fix is [PR #111](https://github.com/olliz0r/sys-botbase/pull/111) by quinnfrost.
Tested on an Erista Switch, HOS 22.5.0, Atmosphere 1.12.0, sys-botbase 2.6.
