// just dispatch for now. real argument parsing when it matters
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "../socket.hpp"
#include "../input.hpp"

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: botbase HOST COMMAND [ARGS...]\n");
        return 2;
    }
    std::string host = argv[1];
    std::string cmd = argv[2];

    try {
        botbase::Socket s(host, 6000);
        if (cmd == "click") {
            if (argc < 4) return 2;
            auto name = botbase::normalize_button(argv[3]);
            std::string line = "click " + name + "\r\n";
            s.send_all(line);
            s.send_all("getVersion\r\n");
            char buf[64];
            s.recv(buf, sizeof buf);
        } else if (cmd == "version") {
            s.send_all("getVersion\r\n");
            char buf[64];
            auto n = s.recv(buf, sizeof buf);
            fwrite(buf, 1, n, stdout);
        }
    } catch (const std::exception& e) {
        fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
