# Quirks and traps

Each item below came from reading sys-botbase's source or from testing on hardware
(HOS 22.5.0, Atmosphere 1.12.0, sys-botbase 2.6). The client handles the ones it can.

## Protocol

**Write commands answer with nothing.** A client cannot tell when they finished or whether they
worked. This client sends `getVersion` after each silent command and waits for its reply. The
main loop reads one line per wake-up, byte by byte, in order, so the reply proves the command
ran. The command and its fence go out in one write; sys-botbase leaves the second line in the
socket until its next wake-up.

**One command per 50 ms.** The main loop sleeps `mainLoopSleepTime` (50 ms) after each wake-up.
`configure mainLoopSleepTime 0` (`--fast`, `Client::fast_mode()`) cuts a round trip from about 50 ms
to about 25 ms. Upstream issue #40 is the same finding for analog sticks.

**Settings are global.** `configure` changes state inside sys-botbase. It affects every client
and lasts until the console reboots.

**Output goes to the last client that spoke.** sys-botbase redirects stdout to the socket of the
command it is running. A `clickSeq` prints `done` whenever it finishes, into whichever socket
that is. This client counts and skips `done` lines wherever they appear.

**Failed reads answer with an empty line.** That covers an unmapped address, no running game and
a busy debugger. The client raises `MemoryAccessError`, or returns an empty `std::optional`.

**Lines are capped at 22016 characters** (`MAX_LINE_LENGTH`), and sys-botbase does not check
the bound: a longer line writes past its buffer. The client refuses long lines, and `type`
splits long text.

## Buttons and input

**An unknown button name presses A.** `parseStringToButton` falls through to `HidNpadButton_A`.
A typo such as `click PLSU` presses A on the console. The client refuses unknown names.

**`clickSeq` can split button names (upstream bug).** `clickSequence()` calls
`strtok(seq, &delim)` where `delim` is a single `char`, not a string. `strtok` keeps reading
delimiter characters past it until it meets a zero byte on the stack. Any letter that happens
to sit there also splits tokens, so a name such as `PLUS` can break into pieces that fall back
to A. The stack contents change between boots, which matches issue #93 ("PLUS registers as A",
"reboot shuffles it"). `botbase seq` runs the steps from the client instead, one command each,
and avoids the bug. Use `seq --server` only when sub-50 ms spacing matters, and check the result.

**The virtual controller takes a player slot.** The first input attaches a virtual Pro
Controller (it shows as player 1 on the HOME menu). Run `botbase detach` (or pass `--detach`) when done.

**Some screens ignore a Pro Controller completely.** A game that only supports Joy-Cons (tested:
WarioWare: Move It!) opens the "Press L + R" controller screen. That screen drops every input
from an unsupported controller, HOME included. `botbase controller joycon-right` made HOME
work again. Switch back with `botbase controller pro`.

**The first HOME after attaching can be dropped** (issue #67). It worked first time in testing
here; `click UNUSED` first attaches the controller with an input games ignore.

**Touch works docked.** Taps reached the HOME menu and the system keyboard with the console
docked. On the HOME menu the first tap on an icon focuses it; A or a second tap opens it.

**The system keyboard ignores the Shift modifier from `keyMod`.** `keyMod 27 1` (x with Shift)
typed a lowercase x. `keyMulti 225 4` (LeftShift and a together) typed A. `type_text` uses
chords for that reason. `X` cancels the system keyboard without saving.

**`touchHold` on a key auto-repeats.** Holding backspace for 1.2 s deleted 13 characters.

## Memory access

**Another debugger blocks all game memory work.** sys-botbase attaches to the game with
`svcDebugActiveProcess` for every game query. A process takes one debugger. When something else
holds it, the attach fails with `svc::ResultBusy` (0xF401), `getHeapBase` prints leftover
garbage (`0x4` on HOS 22), and heap reads fail or return wrong data. Title ID, build ID, version
and the main NSO base still work, because they come from `pm` and `ldr`, not from the debugger.
The client raises `DebuggerBusy` when the heap base is not a plausible address.

The usual holder is Atmosphere's cheat engine (`dmnt`). **SaltyNX forces it on.** SaltyNX's
`checkIfDmntChtNeedsToBeActive()` calls `dmntchtForceOpenCheatProcess()` for any game whose
`atmosphere/contents/<title id>/cheats` folder has an entry, whether or not a cheat matches the
running build. Holding L at launch (Atmosphere's cheat-skip key) does not help, because SaltyNX
forces the attach afterwards. Tested on Super Smash Bros. Ultimate: busy with L held. Tested on
a game with no cheats folder: full access. Options: rename that game's `cheats` folder, disable
SaltyNX, or read memory through dmnt's own `dmnt:cht` service instead.

**The heap region is not mapped from its start.** `getHeapBase` returns the region start. In the
game tested, offsets below 0x40000 were unmapped and reads there failed. Probe before assuming.

**Every game query pauses the game briefly.** Each `getTitleID`, `peek`, `poke` and so on
attaches the debugger and detaches again. Fine for occasional reads; do not poll in a tight
loop during gameplay.

**Memory writes on an online game are a ban risk.** Do not poke while connected to Nintendo's
servers.

## The console

**Port 6000 has no authentication.** Anyone who can reach it can press buttons, read and write
game memory, and take screenshots. Keep it on a trusted LAN.

**Local wireless play turns Wi-Fi off,** which cuts sys-botbase off (issue #10). ldn_mitm keeps
Wi-Fi up but breaks local play in some games.

**A VPN that routes the LAN subnet hides the console.** If a WireGuard or similar tunnel claims
the same subnet as the console's LAN, connections go into the tunnel. Bind to the physical
interface: `--interface en0`, or `ConnectOptions::interface` in the library (macOS `IP_BOUND_IF`, Linux
`SO_BINDTODEVICE`).

**The console must be awake.** Sleep mode stops sysmodules and drops Wi-Fi.

**Even a crashed boot answers.** sys-botbase can keep answering `getVersion` while the console
sits on an Atmosphere fatal screen. A reply proves nothing about boot health; look for new files
in `atmosphere/crash_reports` and `atmosphere/fatal_reports` instead.

**macOS Local Network privacy can block the client.** A process without Local Network permission
gets "No route to host" for every LAN address, while internet addresses still connect. If
`discover` finds nothing and `ping` reaches the console, allow your terminal app under System
Settings, Privacy & Security, Local Network.
