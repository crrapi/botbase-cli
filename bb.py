import socket, sys, time

HOST = "192.168.1.50"
BUTTONS = {"A","B","X","Y","L","R","ZL","ZR","PLUS","MINUS","HOME","CAPTURE",
           "DUP","DDOWN","DLEFT","DRIGHT","LSTICK","RSTICK","PALMA"}

# sys-botbase quietly presses A for any name it doesn't know. so a typo
# in a button becomes an A press on the console. check locally first.
def norm(b):
    b = b.upper()
    if b not in BUTTONS:
        raise ValueError(b)
    return b

class BB:
    def __init__(self, host=HOST):
        self.s = socket.socket()
        self.s.connect((host, 6000))
        self.buf = b""

    def send(self, line):
        self.s.sendall(line.encode() + b"\r\n")

    def recv(self):
        while b"\n" not in self.buf:
            self.buf += self.s.recv(4096)
        line, self.buf = self.buf.split(b"\n", 1)
        return line.decode().rstrip("\r")

    def click(self, b):
        self.send("click " + norm(b))

    def press(self, b):
        self.send("press " + norm(b))

    def release(self, b):
        self.send("release " + norm(b))

    def peek(self, off, size):
        self.send(f"peek 0x{off:x} 0x{size:x}")
        return bytes.fromhex(self.recv())

    def poke(self, off, data):
        self.send(f"poke 0x{off:x} 0x{data.hex()}")
