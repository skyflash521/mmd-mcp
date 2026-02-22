#include "mcp_server.h"
#include <windows.h>

using json = nlohmann::json;

static constexpr const char* PROTOCOL_VERSION = "2025-11-25";

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

    server_.Get("/mcp", [](const httplib::Request&, httplib::Response& res) {
        res.status = 405;
        res.set_header("Allow", "POST");
    });

    server_.Post("/mcp", [this](const httplib::Request& req, httplib::Response& res) {
        handleMcp(req, res);
    });

}

void McpServer::handleMcp(const httplib::Request& req, httplib::Response& res) {
    try {
        handleMcpInner(req, res);
    } catch (const std::exception& e) {
        res.status = 500;
        res.set_content(
            makeError(nullptr, -32603, std::string("Internal error: ") + e.what()).dump(),
            "application/json");
    } catch (...) {
        res.status = 500;
        res.set_content(
            makeError(nullptr, -32603, "Internal error").dump(),
            "application/json");
    }
}

void McpServer::handleMcpInner(const httplib::Request& req, httplib::Response& res) {
    // Origin検証: ブラウザからのクロスオリジンリクエストを拒否
    auto origin = req.headers.find("Origin");
    if (origin != req.headers.end()) {
        res.status = 403;
        res.set_content(
            makeError(nullptr, -32600, "Forbidden: Origin header not allowed").dump(),
            "application/json");
        return;
    }

    json body;
    try {
        body = json::parse(req.body);
    } catch (const json::parse_error&) {
        res.status = 400;
        res.set_content(makeError(nullptr, -32700, "Parse error").dump(), "application/json");
        return;
    }

    if (!body.is_object() || body.value("jsonrpc", "") != "2.0") {
        res.status = 400;
        res.set_content(makeError(nullptr, -32600, "Invalid Request").dump(), "application/json");
        return;
    }

    auto method = body.value("method", "");
    bool isNotification = !body.contains("id");
    auto id = body.contains("id") ? body["id"] : json(nullptr);

    if (method == "initialize") {
        std::string toolList;
        for (auto& tool : tools_) {
            toolList += "- " + tool->name() + ": " + tool->description() + "\n";
        }

        json result = {
            {"protocolVersion", PROTOCOL_VERSION},
            {"capabilities", {{"tools", json::object()}}},
            {"serverInfo", {{"name", "mmd-mcp"}, {"version", "0.1.0"}}},
            {"instructions",
                "MCP server for MikuMikuDance (MMD). "
                "Provides tools to read/write MMD's internal state via direct memory access. "
                "Use tools/list for full tool definitions and input schemas.\n\n"
                "Available tools:\n" + toolList + "\n"
                "Units: rotation values are in radians (e.g. 0.1745 rad = 10 degrees). "
                "Position and distance are in MMD internal units.\n\n"
                "Frame range syntax: \"0\" (single), \"1-10\" (range), \"1-3,5,8-10\" (mixed). "
                "Frame 0 is the base keyframe and cannot be deleted."
            }
        };
        res.set_content(makeResponse(id, result).dump(), "application/json");
        return;
    }

    // initialize以降のリクエストはMCP-Protocol-Versionヘッダを検証
    // （ここに来る時点でinitialize済み。通知はヘッダ不要）
    if (!isNotification) {
        auto it = req.headers.find("MCP-Protocol-Version");
        if (it != req.headers.end() && it->second != PROTOCOL_VERSION) {
            res.status = 400;
            res.set_content(
                makeError(id, -32600,
                    "Unsupported protocol version: " + it->second).dump(),
                "application/json");
            return;
        }
    }

    // 通知（idなし）は202で受理。notifications/initialized以外の未知通知も受理。
    if (isNotification) {
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
                try {
                    json result = tool->execute(args);
                    res.set_content(makeResponse(id, result).dump(), "application/json");
                } catch (const std::exception& e) {
                    json result = {
                        {"content", json::array({{{"type", "text"}, {"text", std::string("Internal error: ") + e.what()}}})},
                        {"isError", true}
                    };
                    res.set_content(makeResponse(id, result).dump(), "application/json");
                }
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
