import socket

def send(line):
    s = socket.socket()
    s.connect(("192.168.1.50", 6000))
    s.sendall(line.encode() + b"\r\n")
    return s.recv(4096)

print(send("getVersion"))
print(send("click A"))
