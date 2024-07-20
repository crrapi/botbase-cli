# notes

- sys-botbase port 6000. no auth. anyone on the lan can drive the console
- write commands dont reply. use getVersion fence
- unknown button names press A. always validate before sending
- settings are global and persist until reboot
- 0 memory writes online. getting banned isnt worth it

todo:
- discovery
- tests
- better key coverage


2023-02: HUGE time sink.

tried to read memory on smash. getHeapBase returns 0x4. weird number.
wasnt that way before. turns out dmnt (atmospheres cheat engine) attaches
to the game and sys-botbase cant get its debugger. svc::ResultBusy
(0xF401). getHeapBase prints whatever was in the output var when the
attach failed which is 0x4 on hos 22.

dmnt should only attach when the cheat key is held but saltynx forces
it on for any game with a cheats folder. even if no cheat matches the
build. holding L at launch doesnt help - saltynx forces the attach AFTER.

workaround: rename the cheats folder for that game. or disable saltynx.
or use dmnt:cht service instead.


2023-04: wireguard routes the switchs subnet so every connect goes into
the tunnel. nothing answers. need to pin the socket to the physical
interface.


2024-01: updated to hos 22.1.0 and now sys-botbase crashes at boot
around 30s in. error 2001-0132. thats svc::ResultLimitReached. memory.

atmosphere gives homebrew sysmodules 32MB of extra system memory,
shared. newer hos uses more of its own system pool. less margin.
sys-botbase official 2.5 wants ~4.7MB image + ~2MB socket buffers.

happens in am or hid depending on which process allocates last.
disabling two sysmodules i wasnt using fixed it (recovered ~2.2MB).


2024-07: python is fine. its also slow. a click round trip is 50ms
server side but my python adds another 5-10ms. mostly fine. but ctrl-c
leaves buttons held because the signal is caught mid-wait and there is
no good way to unwind.

also theres no proper type safety anywhere. button names string typed.
every dsl thing reinvented.

been thinking about rewriting in c++. not because python is bad but
because i want the single binary, real signal handling, and no interp
startup. maybe try just the socket + fence layer first.
