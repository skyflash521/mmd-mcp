#include "mcp_server.h"
#include <windows.h>

McpServer::McpServer() {
    server_.Get("/", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(R"({"status":"ok"})", "application/json");
    });
}

void McpServer::start() {
    if (running_) return;
    running_ = true;

    server_thread_ = std::thread([this]() {
        OutputDebugStringA("mmd_mcp: listening on 127.0.0.1:3939\n");
        server_.listen("127.0.0.1", 3939);
    });
}

void McpServer::stop() {
    if (!running_) return;
    running_ = false;

    server_.stop();
    if (server_thread_.joinable()) {
        server_thread_.join();
    }
    OutputDebugStringA("mmd_mcp: server stopped\n");
}
