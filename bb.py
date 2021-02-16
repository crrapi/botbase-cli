import socket

s = socket.socket()
s.connect(("192.168.1.50", 6000))
s.sendall(b"getVersion\r\n")
print(s.recv(32))
