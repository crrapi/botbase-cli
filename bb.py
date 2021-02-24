import socket, sys

HOST = "192.168.1.50"
BUTTONS = {"A","B","X","Y","L","R","ZL","ZR","PLUS","MINUS","HOME","CAPTURE",
           "DUP","DDOWN","DLEFT","DRIGHT","LSTICK","RSTICK","PALMA"}

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
        assert b in BUTTONS, b
        self.send(f"click {b}")

    def peek(self, off, size):
        self.send(f"peek 0x{off:x} 0x{size:x}")
        return bytes.fromhex(self.recv())

    def poke(self, off, data):
        self.send(f"poke 0x{off:x} 0x{data.hex()}")
