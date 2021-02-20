# cmd line: python bb.py click A
import socket, sys

HOST = "192.168.1.50"
BUTTONS = {"A","B","X","Y","L","R","ZL","ZR","PLUS","MINUS","HOME","CAPTURE",
           "DUP","DDOWN","DLEFT","DRIGHT","LSTICK","RSTICK"}

class BB:
    def __init__(self, host=HOST):
        self.s = socket.socket()
        self.s.connect((host, 6000))

    def send(self, line):
        self.s.sendall(line.encode() + b"\r\n")

    def recv(self):
        buf = b""
        while b"\n" not in buf:
            buf += self.s.recv(4096)
        return buf.split(b"\n", 1)[0].decode()

    def click(self, b):
        assert b in BUTTONS
        self.send(f"click {b}")


if __name__ == "__main__":
    bb = BB()
    bb.click(sys.argv[1])
