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
