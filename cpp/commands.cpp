// TODO: actual arg parsing. for now just the bare minimum
#include "../socket.hpp"
#include "../input.hpp"
#include "../protocol.hpp"
#include "../signals.hpp"

#include <cstdio>
#include <cstring>
#include <string>

namespace botbase::cli {

int run_click(botbase::Socket& s, int argc, char** argv) {
    for (int i = 0; i < argc; i++) {
        auto name = botbase::normalize_button(argv[i]);
        std::string line = "click " + name + "\r\n";
        s.send_all(line);
        s.send_all("getVersion\r\n");
        char buf[64];
        s.recv(buf, sizeof buf);
    }
    return 0;
}

int run_peek(botbase::Socket& s, int argc, char** argv) {
    if (argc < 2) return 2;
    char cmd[128];
    snprintf(cmd, sizeof cmd, "peek %s %s\r\n", argv[0], argv[1]);
    s.send_all(cmd);
    char buf[65536];
    auto n = s.recv(buf, sizeof buf);
    fwrite(buf, 1, n, stdout);
    return 0;
}

}
