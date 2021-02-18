import socket

HOST = "192.168.1.50"
BUTTONS = ["A", "B", "X", "Y", "L", "R", "ZL", "ZR", "PLUS", "MINUS", "HOME"]

def send(line):
    s = socket.socket()
    s.connect((HOST, 6000))
    s.sendall(line.encode() + b"\r\n")
    return s.recv(4096)

def click(b):
    send(f"click {b}")

if __name__ == "__main__":
    import sys
    click(sys.argv[1])
