#pragma once

#include <httplib.h>
#include <nlohmann/json.hpp>
#include <thread>
#include <string>

class McpServer {
public:
    McpServer(int port = 3939);
    void start();
    void stop();

private:
    void handleMcp(const httplib::Request& req, httplib::Response& res);

    httplib::Server server_;
    std::thread server_thread_;
    int port_;
    bool running_ = false;
    std::string session_id_;
};
