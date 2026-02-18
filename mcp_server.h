#pragma once

#include <httplib.h>
#include <thread>

class McpServer {
public:
    McpServer();
    void start();
    void stop();

private:
    httplib::Server server_;
    std::thread server_thread_;
    bool running_ = false;
};
