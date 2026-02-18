#include "mcp_server.h"
#include <windows.h>
#include <random>
#include <sstream>
#include <iomanip>

using json = nlohmann::json;

static std::string generateSessionId() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dist(0, 0xFFFFFFFF);

    std::ostringstream ss;
    ss << std::hex << std::setfill('0');
    ss << std::setw(8) << dist(gen) << "-";
    ss << std::setw(4) << (dist(gen) & 0xFFFF) << "-";
    ss << std::setw(4) << ((dist(gen) & 0x0FFF) | 0x4000) << "-";
    ss << std::setw(4) << ((dist(gen) & 0x3FFF) | 0x8000) << "-";
    ss << std::setw(8) << dist(gen) << std::setw(4) << (dist(gen) & 0xFFFF);
    return ss.str();
}

static json makeResponse(const json& id, const json& result) {
    return {{"jsonrpc", "2.0"}, {"id", id}, {"result", result}};
}

static json makeError(const json& id, int code, const std::string& message) {
    return {{"jsonrpc", "2.0"}, {"id", id}, {"error", {{"code", code}, {"message", message}}}};
}

void McpServer::registerTool(std::unique_ptr<ITool> tool) {
    auto toolName = tool->name();
    for (auto& t : tools_) {
        if (t->name() == toolName) return;
    }
    tools_.push_back(std::move(tool));
}

McpServer::McpServer(int port) : port_(port) {
    server_.set_logger([](const httplib::Request& req, const httplib::Response& res) {
        std::string msg = "mmd_mcp: " + req.method + " " + req.path +
                          " -> " + std::to_string(res.status) + "\n";
        OutputDebugStringA(msg.c_str());
    });

    server_.Get("/", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(R"({"status":"ok"})", "application/json");
    });

    server_.Post("/mcp", [this](const httplib::Request& req, httplib::Response& res) {
        handleMcp(req, res);
    });

}

void McpServer::handleMcp(const httplib::Request& req, httplib::Response& res) {
    json body;
    try {
        body = json::parse(req.body);
    } catch (const json::parse_error&) {
        res.status = 400;
        res.set_content(makeError(nullptr, -32700, "Parse error").dump(), "application/json");
        return;
    }

    if (!body.is_object()) {
        res.status = 400;
        res.set_content(makeError(nullptr, -32600, "Invalid Request").dump(), "application/json");
        return;
    }

    auto method = body.value("method", "");
    auto id = body.contains("id") ? body["id"] : json(nullptr);

    if (method == "initialize") {
        if (session_id_.empty()) {
            session_id_ = generateSessionId();
        }
        res.set_header("MCP-Session-Id", session_id_);

        json result = {
            {"protocolVersion", "2025-11-25"},
            {"capabilities", {{"tools", json::object()}}},
            {"serverInfo", {{"name", "mmd-mcp"}, {"version", "0.1.0"}}}
        };
        res.set_content(makeResponse(id, result).dump(), "application/json");
        return;
    }

    // initialize以外はセッションIDの検証
    if (session_id_.empty()) {
        res.status = 400;
        res.set_content(makeError(id, -32600, "Not initialized").dump(), "application/json");
        return;
    }
    auto it = req.headers.find("MCP-Session-Id");
    if (it == req.headers.end() || it->second != session_id_) {
        res.status = 400;
        res.set_content(makeError(id, -32600, "Invalid session").dump(), "application/json");
        return;
    }

    if (method == "notifications/initialized") {
        res.status = 202;
        return;
    }

    if (method == "tools/list") {
        json tools = json::array();
        for (auto& tool : tools_) {
            tools.push_back({
                {"name", tool->name()},
                {"description", tool->description()},
                {"inputSchema", tool->inputSchema()}
            });
        }
        res.set_content(makeResponse(id, {{"tools", tools}}).dump(), "application/json");
        return;
    }

    if (method == "tools/call") {
        if (!body.contains("params") || !body["params"].is_object()) {
            res.set_content(makeError(id, -32602, "Invalid params").dump(), "application/json");
            return;
        }
        auto toolName = body["params"].value("name", "");
        if (body["params"].contains("arguments") && !body["params"]["arguments"].is_object()) {
            res.set_content(makeError(id, -32602, "Invalid arguments").dump(), "application/json");
            return;
        }
        auto args = body["params"].value("arguments", json::object());
        for (auto& tool : tools_) {
            if (tool->name() == toolName) {
                json result = tool->execute(args);
                res.set_content(makeResponse(id, result).dump(), "application/json");
                return;
            }
        }
        res.set_content(makeError(id, -32602, "Unknown tool: " + toolName).dump(), "application/json");
        return;
    }

    res.set_content(makeError(id, -32601, "Method not found: " + method).dump(), "application/json");
}

void McpServer::start() {
    if (running_) return;
    running_ = true;

    server_thread_ = std::thread([this]() {
        OutputDebugStringA("mmd_mcp: listening on 127.0.0.1\n");
        server_.listen("127.0.0.1", port_);
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
