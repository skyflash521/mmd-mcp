#pragma once

#include "tool.h"
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <thread>
#include <string>
#include <vector>
#include <memory>

class McpServer {
public:
    McpServer(int port = 3939);
    void registerTool(std::unique_ptr<ITool> tool);
    void start();
    void stop();

private:
    void handleMcp(const httplib::Request& req, httplib::Response& res);
    void handleMcpInner(const httplib::Request& req, httplib::Response& res);

    httplib::Server server_;
    std::thread server_thread_;
    int port_;
    bool running_ = false;
    std::vector<std::unique_ptr<ITool>> tools_;
};
