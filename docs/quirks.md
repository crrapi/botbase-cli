# quirks

everything found testing on an erista on hos 22. the client hides what it can.

## protocol

**write commands never reply.** there is no way to tell when they
finished. this client sends getVersion after each write and waits for
its reply. one line per wakeup, in order.

**one command per 50ms.** the main loop sleeps `mainLoopSleepTime` after
each cycle. set it to 0 to halve the latency.

**settings are global.** configure state lives in sys-botbase, affects
every client, and resets at reboot.

**the last client gets the output.** sys-botbase redirects stdout to the
socket of whatever is running right now. clickSeq prints "done" at the
end, into whichever socket was last. this leaks into replies.

## buttons

**unknown names press A.** parseStringToButton falls through to A.
`click PLSU` presses A. the client refuses unknown names before sending.

**clickSeq can split button names.** upstream #93. strtok with a char
delim keeps reading past it. PLUS can break into pieces, each pressing A.
use the client-side sequence instead.

## memory

**saltynx forces dmnt onto games with a cheats folder.** blocks memory
reads. workaround: rename the folder, or disable saltynx.

**low heap base = debugger busy.** `getHeapBase` prints garbage left in
svcGetInfo's output var when the attach fails. on hos 22 thats 0x4.

## network

**port 6000 has no auth.** anyone on the lan can press buttons, read
memory, dump screenshots.

**vpn subnet collision hides the console.** wireguard routes the same
subnet, outgoing connects go into the tunnel. bind to the physical
interface.

**sleep mode kills the service.** wifi drops and sys-botbase stops. the
console must be awake.
