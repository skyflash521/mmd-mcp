#include "mcp_server.h"
#include "tools/ping_tool.h"
#include "tests/mock_frame.h"
#include "tools/frame/frame_tool.h"
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <cassert>
#include <thread>
#include <chrono>
#include <cstdio>

using json = nlohmann::json;

static const int TEST_PORT = 13939;
static httplib::Client cli("127.0.0.1", TEST_PORT);
static std::string sessionId;

static void test_initialize() {
    auto res = cli.Post("/mcp",
        R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-11-25","capabilities":{},"clientInfo":{"name":"test","version":"1.0"}}})",
        "application/json");
    assert(res && res->status == 200);

    auto body = json::parse(res->body);
    assert(body["jsonrpc"] == "2.0");
    assert(body["id"] == 1);
    assert(body["result"]["protocolVersion"] == "2025-11-25");
    assert(body["result"]["serverInfo"]["name"] == "mmd-mcp");
    assert(body["result"]["capabilities"].contains("tools"));

    sessionId = res->get_header_value("MCP-Session-Id");
    assert(!sessionId.empty());

    printf("  PASS: initialize\n");
}

static void test_reinitialize_keeps_session() {
    auto res = cli.Post("/mcp",
        R"({"jsonrpc":"2.0","id":99,"method":"initialize","params":{"protocolVersion":"2025-11-25","capabilities":{},"clientInfo":{"name":"test","version":"1.0"}}})",
        "application/json");
    assert(res && res->status == 200);

    auto newSessionId = res->get_header_value("MCP-Session-Id");
    assert(newSessionId == sessionId);

    printf("  PASS: reinitialize keeps session\n");
}

static void test_initialized_notification() {
    httplib::Headers headers = {{"MCP-Session-Id", sessionId}};
    auto res = cli.Post("/mcp", headers,
        R"({"jsonrpc":"2.0","method":"notifications/initialized"})",
        "application/json");
    assert(res && res->status == 202);

    printf("  PASS: notifications/initialized\n");
}

static void test_tools_list() {
    httplib::Headers headers = {{"MCP-Session-Id", sessionId}};
    auto res = cli.Post("/mcp", headers,
        R"({"jsonrpc":"2.0","id":2,"method":"tools/list"})",
        "application/json");
    assert(res && res->status == 200);

    auto body = json::parse(res->body);
    auto& tools = body["result"]["tools"];
    assert(tools.is_array() && tools.size() >= 3);

    auto findTool = [&](const std::string& name) -> const json* {
        for (auto& t : tools) {
            if (t["name"] == name) return &t;
        }
        return nullptr;
    };

    for (const auto& name : {"ping", "get_frame", "set_frame"}) {
        auto* t = findTool(name);
        assert(t != nullptr);
        assert((*t)["description"].is_string());
        assert((*t)["inputSchema"].is_object());
    }

    printf("  PASS: tools/list\n");
}

static void test_tools_call_ping() {
    httplib::Headers headers = {{"MCP-Session-Id", sessionId}};
    auto res = cli.Post("/mcp", headers,
        R"({"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"ping","arguments":{}}})",
        "application/json");
    assert(res && res->status == 200);

    auto body = json::parse(res->body);
    assert(body["result"]["content"][0]["type"] == "text");
    assert(body["result"]["content"][0]["text"] == "pong");
    assert(body["result"]["isError"] == false);

    printf("  PASS: tools/call ping\n");
}

static void test_tools_call_unknown() {
    httplib::Headers headers = {{"MCP-Session-Id", sessionId}};
    auto res = cli.Post("/mcp", headers,
        R"({"jsonrpc":"2.0","id":4,"method":"tools/call","params":{"name":"nonexistent","arguments":{}}})",
        "application/json");
    assert(res && res->status == 200);

    auto body = json::parse(res->body);
    assert(body.contains("error"));
    assert(body["error"]["code"] == -32602);

    printf("  PASS: tools/call unknown tool\n");
}

static void test_method_not_found() {
    httplib::Headers headers = {{"MCP-Session-Id", sessionId}};
    auto res = cli.Post("/mcp", headers,
        R"({"jsonrpc":"2.0","id":5,"method":"unknown/method"})",
        "application/json");
    assert(res && res->status == 200);

    auto body = json::parse(res->body);
    assert(body["error"]["code"] == -32601);

    printf("  PASS: method not found\n");
}

static void test_invalid_session() {
    httplib::Headers headers = {{"MCP-Session-Id", "invalid-session-id"}};
    auto res = cli.Post("/mcp", headers,
        R"({"jsonrpc":"2.0","id":6,"method":"tools/list"})",
        "application/json");
    assert(res && res->status == 400);

    printf("  PASS: invalid session\n");
}

static void test_not_initialized() {
    // MCP-Session-Id header absent, session exists
    auto res = cli.Post("/mcp",
        R"({"jsonrpc":"2.0","id":7,"method":"tools/list"})",
        "application/json");
    assert(res && res->status == 400);

    auto body = json::parse(res->body);
    assert(body["error"]["code"] == -32600);

    printf("  PASS: missing session header\n");
}

static void test_invalid_params() {
    httplib::Headers headers = {{"MCP-Session-Id", sessionId}};
    auto res = cli.Post("/mcp", headers,
        R"({"jsonrpc":"2.0","id":8,"method":"tools/call"})",
        "application/json");
    assert(res && res->status == 200);

    auto body = json::parse(res->body);
    assert(body["error"]["code"] == -32602);

    printf("  PASS: tools/call missing params\n");
}

static void test_parse_error() {
    auto res = cli.Post("/mcp", "not json", "application/json");
    assert(res && res->status == 400);

    auto body = json::parse(res->body);
    assert(body["error"]["code"] == -32700);

    printf("  PASS: parse error\n");
}

static void test_invalid_request() {
    auto res = cli.Post("/mcp", R"([1,2,3])", "application/json");
    assert(res && res->status == 400);

    auto body = json::parse(res->body);
    assert(body["error"]["code"] == -32600);

    printf("  PASS: invalid request (non-object JSON)\n");
}

static void test_not_initialized_fresh() {
    // 新しいサーバーで初期化前にリクエスト
    McpServer server2(TEST_PORT + 1);
    server2.start();
    httplib::Client cli2("127.0.0.1", TEST_PORT + 1);
    for (int i = 0; i < 50; ++i) {
        auto r = cli2.Get("/");
        if (r && r->status == 200) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    auto res = cli2.Post("/mcp",
        R"({"jsonrpc":"2.0","id":1,"method":"tools/list"})",
        "application/json");
    assert(res && res->status == 400);

    auto body = json::parse(res->body);
    assert(body["error"]["code"] == -32600);

    server2.stop();
    printf("  PASS: not initialized (400)\n");
}

static MockFrameReader g_reader;
static MockFrameWriter g_writer;

static void test_tools_call_get_frame() {
    httplib::Headers headers = {{"MCP-Session-Id", sessionId}};
    auto res = cli.Post("/mcp", headers,
        R"({"jsonrpc":"2.0","id":20,"method":"tools/call","params":{"name":"get_frame","arguments":{}}})",
        "application/json");
    assert(res && res->status == 200);

    auto body = json::parse(res->body);
    assert(body["result"]["isError"] == false);
    assert(body["result"]["content"][0]["text"] == "42");

    printf("  PASS: tools/call get_frame\n");
}

static void test_tools_call_set_frame() {
    httplib::Headers headers = {{"MCP-Session-Id", sessionId}};
    auto res = cli.Post("/mcp", headers,
        R"({"jsonrpc":"2.0","id":21,"method":"tools/call","params":{"name":"set_frame","arguments":{"frame":100}}})",
        "application/json");
    assert(res && res->status == 200);

    auto body = json::parse(res->body);
    assert(body["result"]["isError"] == false);
    assert(body["result"]["content"][0]["text"] == "Frame set to 100");
    assert(g_writer.written() == 100);

    printf("  PASS: tools/call set_frame\n");
}

static void test_tools_call_set_frame_negative() {
    httplib::Headers headers = {{"MCP-Session-Id", sessionId}};
    auto res = cli.Post("/mcp", headers,
        R"({"jsonrpc":"2.0","id":22,"method":"tools/call","params":{"name":"set_frame","arguments":{"frame":-1}}})",
        "application/json");
    assert(res && res->status == 200);

    auto body = json::parse(res->body);
    assert(body["result"]["isError"] == true);

    printf("  PASS: tools/call set_frame negative\n");
}

int main() {
    g_reader.set(42);

    McpServer server(TEST_PORT);
    server.registerTool(std::make_unique<PingTool>());
    server.registerTool(std::make_unique<GetFrameTool>(&g_reader));
    server.registerTool(std::make_unique<SetFrameTool>(&g_writer));
    server.start();
    for (int i = 0; i < 50; ++i) {
        auto r = cli.Get("/");
        if (r && r->status == 200) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    printf("Running MCP tests...\n");

    test_initialize();
    test_reinitialize_keeps_session();
    test_initialized_notification();
    test_tools_list();
    test_tools_call_ping();
    test_tools_call_unknown();
    test_method_not_found();
    test_invalid_session();
    test_not_initialized();
    test_invalid_params();
    test_parse_error();
    test_invalid_request();
    test_not_initialized_fresh();
    test_tools_call_get_frame();
    test_tools_call_set_frame();
    test_tools_call_set_frame_negative();

    server.stop();

    printf("All %d tests passed.\n", 16);
    return 0;
}
