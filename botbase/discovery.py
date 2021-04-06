import socket, ipaddress
from concurrent.futures import ThreadPoolExecutor

def probe(host, port=6000, timeout=0.5):
    s = socket.socket()
    s.settimeout(timeout)
    try:
        s.connect((host, port))
        s.sendall(b"getVersion\r\n")
        s.settimeout(1.5)
        data = s.recv(32)
        line = data.split(b"\n", 1)[0].decode().strip()
        parts = line.split(".")
        if all(p.isdigit() for p in parts):
            return line
    except OSError:
        return None
    finally:
        s.close()


def discover(subnet):
    net = ipaddress.ip_network(subnet, strict=False)
    with ThreadPoolExecutor(max_workers=64) as pool:
        return [(str(h), v) for h, v in zip(net.hosts(), pool.map(probe, map(str, net.hosts()))) if v]
