// just seeing if this builds
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>

int main(int argc, char** argv) {
    if (argc != 2) { fprintf(stderr, "usage: %s HOST\n", argv[0]); return 2; }
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port = htons(6000);
    inet_pton(AF_INET, argv[1], &a.sin_addr);
    if (connect(fd, (sockaddr*)&a, sizeof a) < 0) { perror("connect"); return 1; }
    const char* cmd = "getVersion\r\n";
    send(fd, cmd, strlen(cmd), 0);
    char buf[64];
    ssize_t n = recv(fd, buf, sizeof buf, 0);
    if (n > 0) fwrite(buf, 1, n, stdout);
    close(fd);
}
