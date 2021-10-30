import socket, threading, time

HEAP = 0x1000000000
MAIN = 0x7000000000

class FakeServer:
    def __init__(self, port=0):
        self.sock = socket.socket()
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.sock.bind(("127.0.0.1", port))
        self.sock.listen(4)
        self.port = self.sock.getsockname()[1]
        self.lines = []
        self.memory = {}
        self.t = threading.Thread(target=self._serve, daemon=True)
        self.t.start()

    def _serve(self):
        try:
            while True:
                c, _ = self.sock.accept()
                threading.Thread(target=self._client, args=(c,), daemon=True).start()
        except OSError:
            pass

    def _client(self, c):
        buf = b""
        while True:
            try:
                data = c.recv(4096)
            except OSError:
                return
            if not data:
                return
            buf += data
            while b"\n" in buf:
                line, buf = buf.split(b"\n", 1)
                text = line.decode().rstrip("\r")
                self.lines.append(text)
                reply = self._handle(text)
                if reply is not None:
                    c.sendall(reply.encode() + b"\n")

    def _handle(self, text):
        argv = text.split()
        if argv[0] == "getVersion":
            return "2.0"
        if argv[0] == "charge":
            return "87"
        return None
