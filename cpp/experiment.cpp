#include "socket.hpp"
#include <cstdio>

int main(int argc, char** argv) {
    if (argc != 2) { fprintf(stderr, "usage: %s HOST\n", argv[0]); return 2; }
    botbase::Socket s(argv[1], 6000);
    s.send_all("getVersion\r\n");
    char buf[64];
    auto n = s.recv(buf, sizeof buf);
    fwrite(buf, 1, n, stdout);
}
