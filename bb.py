import socket, sys, time, argparse

HOST = "192.168.1.50"
BUTTONS = {"A","B","X","Y","L","R","ZL","ZR","PLUS","MINUS","HOME","CAPTURE",
           "DUP","DDOWN","DLEFT","DRIGHT","LSTICK","RSTICK","PALMA","UNUSED"}
ALIAS = {"UP":"DUP","DOWN":"DDOWN","LEFT":"DLEFT","RIGHT":"DRIGHT",
         "START":"PLUS","SELECT":"MINUS"}

def norm(b):
    b = ALIAS.get(b.upper().strip(), b.upper().strip())
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

    def click(self, b): self.send("click " + norm(b))
    def press(self, b): self.send("press " + norm(b))
    def release(self, b): self.send("release " + norm(b))
    def peek(self, off, size):
        self.send(f"peek 0x{off:x} 0x{size:x}")
        return bytes.fromhex(self.recv())
    def poke(self, off, data):
        self.send(f"poke 0x{off:x} 0x{data.hex()}")


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--host", default=HOST)
    p.add_argument("cmd")
    p.add_argument("args", nargs="*")
    a = p.parse_args()
    bb = BB(a.host)
    if a.cmd == "click":
        for b in a.args: bb.click(b)
    elif a.cmd == "peek":
        print(bb.peek(int(a.args[0], 0), int(a.args[1], 0)).hex())
    else:
        bb.send(a.cmd + " " + " ".join(a.args))
        print(bb.recv())


if __name__ == "__main__":
    main()
