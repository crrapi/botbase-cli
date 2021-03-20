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

    def click(self, b): self.send("click " + norm(b))
    def press(self, b): self.send("press " + norm(b))
    def release(self, b): self.send("release " + norm(b))

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
