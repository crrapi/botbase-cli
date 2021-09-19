import socket
from .constants import BUTTONS, ALIAS, DEFAULT_PORT

def norm(b):
    b = ALIAS.get(b.upper().strip(), b.upper().strip())
    if b not in BUTTONS:
        raise ValueError(f"unknown button {b!r} - sys-botbase would press A")
    return b


class BB:
    def __init__(self, host, port=DEFAULT_PORT):
        self.s = socket.socket()
        self.s.connect((host, port))
        self.buf = b""

    def send(self, line):
        self.s.sendall(line.encode() + b"\r\n")

    def recv(self):
        while b"\n" not in self.buf:
            self.buf += self.s.recv(4096)
        line, self.buf = self.buf.split(b"\n", 1)
        return line.decode().rstrip("\r")

    # sys-botbase write commands give no reply. so you never know when
    # they actually ran. trick: send getVersion right after, and wait for
    # the reply. the main loop handles one line per wakeup in order, so
    # the version reply proves the write was processed.
    def _write(self, line):
        self.send(line)
        self.send("getVersion")
        self.recv()

    def click(self, b): self._write("click " + norm(b))
    def press(self, b): self._write("press " + norm(b))
    def release(self, b): self._write("release " + norm(b))

    def set_stick(self, side, x, y):
        side = side.upper()
        self.send(f"setStick {side} 0x{x & 0xffff:x} 0x{y & 0xffff:x}")

    def peek(self, off, size, region="heap"):
        cmd = {"heap":"peek","main":"peekMain","absolute":"peekAbsolute"}[region]
        self.send(f"{cmd} 0x{off:x} 0x{size:x}")
        r = self.recv()
        return bytes.fromhex(r) if r else None

    def poke(self, off, data, region="heap"):
        cmd = {"heap":"poke","main":"pokeMain","absolute":"pokeAbsolute"}[region]
        self.send(f"{cmd} 0x{off:x} 0x{data.hex()}")

    def pointer(self, chain):
        self.send("pointer " + " ".join(f"0x{c:x}" for c in chain))
        return int(self.recv(), 16)

    def pointer_peek(self, size, chain, final=0):
        args = " ".join(["0x%x" % size] + [f"0x{c:x}" for c in chain] + [f"0x{final:x}"])
        self.send("pointerPeek " + args)
        return bytes.fromhex(self.recv())

    def freeze(self, off, data):
        self.send(f"freeze 0x{off:x} 0x{data.hex()}")

    def unfreeze(self, off):
        self.send(f"unFreeze 0x{off:x}")

    def freeze_count(self):
        self.send("freezeCount")
        return int(self.recv(), 16)

    def touch(self, points):
        for x, y in points:
            assert 0 <= x <= 1280 and 0 <= y <= 720, (x, y)
        args = " ".join(f"{x} {y}" for x, y in points)
        self.send("touch " + args)

    def touch_hold(self, x, y, ms):
        self.send(f"touchHold {x} {y} {ms}")

    def key(self, *codes):
        self.send("key " + " ".join(str(c) for c in codes))

    def type_text(self, text):
        # US layout only for now. shifted chars go as keyMulti chords because
        # keyMod's Shift bit gets ignored by the system keyboard. dumb.
        LETTERS = {c: (4 + i, c.isupper()) for i, c in enumerate("abcdefghijklmnopqrstuvwxyz")}
        for c in text:
            if c in LETTERS:
                code, shift = LETTERS[c]
                if shift:
                    self.send(f"keyMulti 225 {code}")
                else:
                    self.send(f"key {code}")
            elif c == " ":
                self.send("key 44")

    def screenshot(self):
        self.send("pixelPeek")
        return bytes.fromhex(self.recv())

    def sequence(self, steps):
        # steps: strings click, ints wait ms, ("press", "A") etc.
        # clickSeq exists on the server but it has a strtok bug where button
        # names can break into letters. doing it client side is slower but
        # actually works.
        import time
        for step in steps:
            if isinstance(step, int):
                time.sleep(step / 1000)
            elif isinstance(step, str):
                self.click(step)
            else:
                op, *args = step
                getattr(self, op)(*args)

    def configure(self, name, value):
        self._write(f"configure {name} {value}")

    def fast(self):
        self.configure("mainLoopSleepTime", 0)

    def fd_count(self):
        self.send("fdCount")
        return int(self.recv())

    def charge(self):
        self.send("charge")
        return int(self.recv())

    def freeze_clear(self): self._write("freezeClear")
    def freeze_pause(self): self._write("freezePause")
    def freeze_unpause(self): self._write("freezeUnpause")
