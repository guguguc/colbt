#include <csignal>
#include <cstdio>
#include <string>

#include "im/core/icserver.h"

static im::ServerCore* g_server = nullptr;

static void onSignal(int) {
    if (g_server) g_server->stop();
}

int main(int argc, char* argv[]) {
    uint16_t port = 9000;
    std::string dbPath = "im.db";
    if (argc > 1) port = static_cast<uint16_t>(std::stoi(argv[1]));
    if (argc > 2) dbPath = argv[2];

    im::ServerCore server;
    g_server = &server;
    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);

    int rc = server.run(port, dbPath);
    g_server = nullptr;
    return rc;
}
