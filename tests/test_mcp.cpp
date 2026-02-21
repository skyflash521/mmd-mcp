#include "mcp_server.h"
#include "tools/ping_tool.h"
#include "tests/mock_frame.h"
#include "tools/frame/frame_tool.h"
#include "tests/mock_camera.h"
#include "tools/camera/camera_tool.h"
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <cassert>
#include <thread>
#include <chrono>
#include <cstdio>

using json = nlohmann::json;

static int g_passed = 0;
static const int TEST_PORT = 13939;
static httplib::Client cli("127.0.0.1", TEST_PORT);

// initialize後のリクエスト用ヘルパー（MCP-Protocol-Versionヘッダ付き）
static httplib::Headers mcpHeaders() {
    return {{"MCP-Protocol-Version", "2025-11-25"}};
}

static httplib::Result mcpPost(const std::string& body) {
    return cli.Post("/mcp", mcpHeaders(), body, "application/json");
}

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

    ++g_passed; printf("  PASS: initialize\n");
}

static void test_initialized_notification() {
    // notifications/initializedはヘッダ不要（initialize直後の通知）
    auto res = cli.Post("/mcp",
        R"({"jsonrpc":"2.0","method":"notifications/initialized"})",
        "application/json");
    assert(res && res->status == 202);

    ++g_passed; printf("  PASS: notifications/initialized\n");
}

static void test_tools_list() {
    auto res = mcpPost(R"({"jsonrpc":"2.0","id":2,"method":"tools/list"})");
    assert(res && res->status == 200);

    auto body = json::parse(res->body);
    auto& tools = body["result"]["tools"];
    assert(tools.is_array() && tools.size() == 7);  // 二重登録されたpingは1件のみ

    auto findTool = [&](const std::string& name) -> const json* {
        for (auto& t : tools) {
            if (t["name"] == name) return &t;
        }
        return nullptr;
    };

    for (const auto& name : {"ping", "get_frame", "set_frame",
         "get_camera_keyframes", "create_camera_keyframes",
         "update_camera_keyframes", "delete_camera_keyframes"}) {
        auto* t = findTool(name);
        assert(t != nullptr);
        assert((*t)["description"].is_string());
        assert((*t)["inputSchema"].is_object());
    }

    // パラメータなしツールのadditionalProperties検証
    auto* ping = findTool("ping");
    assert((*ping)["inputSchema"]["additionalProperties"] == false);
    auto* getFrame = findTool("get_frame");
    assert((*getFrame)["inputSchema"]["additionalProperties"] == false);

    ++g_passed; printf("  PASS: tools/list\n");
}

static void test_tools_call_ping() {
    auto res = mcpPost(
        R"({"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"ping","arguments":{}}})");
    assert(res && res->status == 200);

    auto body = json::parse(res->body);
    assert(body["result"]["content"][0]["type"] == "text");
    assert(body["result"]["content"][0]["text"] == "pong");
    assert(body["result"]["isError"] == false);

    ++g_passed; printf("  PASS: tools/call ping\n");
}

static void test_tools_call_unknown() {
    auto res = mcpPost(
        R"({"jsonrpc":"2.0","id":4,"method":"tools/call","params":{"name":"nonexistent","arguments":{}}})");
    assert(res && res->status == 200);

    auto body = json::parse(res->body);
    assert(body.contains("error"));
    assert(body["error"]["code"] == -32602);

    ++g_passed; printf("  PASS: tools/call unknown tool\n");
}

static void test_method_not_found() {
    auto res = mcpPost(
        R"({"jsonrpc":"2.0","id":5,"method":"unknown/method"})");
    assert(res && res->status == 200);

    auto body = json::parse(res->body);
    assert(body["error"]["code"] == -32601);

    ++g_passed; printf("  PASS: method not found\n");
}

static void test_invalid_params() {
    auto res = mcpPost(
        R"({"jsonrpc":"2.0","id":8,"method":"tools/call"})");
    assert(res && res->status == 200);

    auto body = json::parse(res->body);
    assert(body["error"]["code"] == -32602);

    ++g_passed; printf("  PASS: tools/call missing params\n");
}

static void test_parse_error() {
    auto res = cli.Post("/mcp", "not json", "application/json");
    assert(res && res->status == 400);

    auto body = json::parse(res->body);
    assert(body["error"]["code"] == -32700);

    ++g_passed; printf("  PASS: parse error\n");
}

static void test_invalid_request() {
    auto res = cli.Post("/mcp", R"([1,2,3])", "application/json");
    assert(res && res->status == 400);

    auto body = json::parse(res->body);
    assert(body["error"]["code"] == -32600);

    ++g_passed; printf("  PASS: invalid request (non-object JSON)\n");
}

// MCP-Protocol-Versionヘッダ検証テスト
static void test_missing_protocol_version_header() {
    // ヘッダなしでtools/listを呼ぶ → 欠落時はスキップ（仕様上SHOULD）なので200
    auto res = cli.Post("/mcp",
        R"({"jsonrpc":"2.0","id":10,"method":"tools/list"})",
        "application/json");
    assert(res && res->status == 200);

    auto body = json::parse(res->body);
    assert(body.contains("result"));

    ++g_passed; printf("  PASS: missing protocol version header\n");
}

static void test_wrong_protocol_version_header() {
    // 不正バージョンでtools/listを呼ぶ → 400
    httplib::Headers headers = {{"MCP-Protocol-Version", "9999-01-01"}};
    auto res = cli.Post("/mcp", headers,
        R"({"jsonrpc":"2.0","id":11,"method":"tools/list"})",
        "application/json");
    assert(res && res->status == 400);

    auto body = json::parse(res->body);
    assert(body["error"]["code"] == -32600);

    ++g_passed; printf("  PASS: wrong protocol version header\n");
}

// Origin検証テスト
static void test_origin_header_rejected() {
    httplib::Headers headers = {
        {"MCP-Protocol-Version", "2025-11-25"},
        {"Origin", "http://evil.example.com"}
    };
    auto res = cli.Post("/mcp", headers,
        R"({"jsonrpc":"2.0","id":12,"method":"tools/list"})",
        "application/json");
    assert(res && res->status == 403);

    ++g_passed; printf("  PASS: origin header rejected\n");
}

// 未知の通知は202で受理
static void test_unknown_notification() {
    auto res = mcpPost(
        R"({"jsonrpc":"2.0","method":"notifications/custom"})");
    assert(res && res->status == 202);

    ++g_passed; printf("  PASS: unknown notification accepted\n");
}

static void test_get_mcp_returns_405() {
    auto res = cli.Get("/mcp");
    assert(res && res->status == 405);
    assert(res->get_header_value("Allow") == "POST");

    ++g_passed; printf("  PASS: GET /mcp returns 405 with Allow: POST\n");
}

static void test_tools_call_arguments_must_be_object() {
    auto res = mcpPost(
        R"({"jsonrpc":"2.0","id":40,"method":"tools/call","params":{"name":"ping","arguments":[1,2,3]}})");
    assert(res && res->status == 200);

    auto body = json::parse(res->body);
    assert(body.contains("error"));
    assert(body["error"]["code"] == -32602);

    ++g_passed; printf("  PASS: tools/call arguments must be object\n");
}

static void test_jsonrpc_field_required() {
    // jsonrpc フィールドなし → Invalid Request
    auto res = cli.Post("/mcp",
        R"({"id":50,"method":"tools/list"})",
        "application/json");
    assert(res && res->status == 400);

    auto body = json::parse(res->body);
    assert(body["error"]["code"] == -32600);

    ++g_passed; printf("  PASS: jsonrpc field required\n");
}

static MockFrameReader g_reader;
static MockFrameWriter g_writer;
static MockCameraAccessor g_cam_accessor;

static void test_tools_call_get_frame() {
    auto res = mcpPost(
        R"({"jsonrpc":"2.0","id":20,"method":"tools/call","params":{"name":"get_frame","arguments":{}}})");
    assert(res && res->status == 200);

    auto body = json::parse(res->body);
    assert(body["result"]["isError"] == false);
    assert(body["result"]["content"][0]["text"] == "42");

    ++g_passed; printf("  PASS: tools/call get_frame\n");
}

static void test_tools_call_set_frame() {
    auto res = mcpPost(
        R"({"jsonrpc":"2.0","id":21,"method":"tools/call","params":{"name":"set_frame","arguments":{"frame":100}}})");
    assert(res && res->status == 200);

    auto body = json::parse(res->body);
    assert(body["result"]["isError"] == false);
    assert(body["result"]["content"][0]["text"] == "Frame set to 100");
    assert(g_writer.written() == 100);

    ++g_passed; printf("  PASS: tools/call set_frame\n");
}

static void test_tools_call_set_frame_negative() {
    auto res = mcpPost(
        R"({"jsonrpc":"2.0","id":22,"method":"tools/call","params":{"name":"set_frame","arguments":{"frame":-1}}})");
    assert(res && res->status == 200);

    auto body = json::parse(res->body);
    assert(body["result"]["isError"] == true);

    ++g_passed; printf("  PASS: tools/call set_frame negative\n");
}

static void test_tools_call_get_camera_keyframes() {
    auto res = mcpPost(
        R"({"jsonrpc":"2.0","id":30,"method":"tools/call","params":{"name":"get_camera_keyframes","arguments":{"frames":"42"}}})");
    assert(res && res->status == 200);

    auto body = json::parse(res->body);
    assert(body["result"]["isError"] == false);
    auto data = json::parse(body["result"]["content"][0]["text"].get<std::string>());
    assert(data["keyframes"].size() == 1);
    assert(data["keyframes"][0]["frame"] == 42);
    assert(data["keyframes"][0]["distance"] == -45.0f);

    ++g_passed; printf("  PASS: tools/call get_camera_keyframes\n");
}

static void test_tools_call_update_camera_keyframes() {
    auto res = mcpPost(
        R"({"jsonrpc":"2.0","id":31,"method":"tools/call","params":{"name":"update_camera_keyframes","arguments":{"keyframes":[{"frame":42,"fov":60}]}}})");
    assert(res && res->status == 200);

    auto body = json::parse(res->body);
    assert(body["result"]["isError"] == false);

    auto* written = g_cam_accessor.get(42);
    assert(written != nullptr);
    assert(written->view_angle == 60);
    // 部分更新: distanceは変更なし
    assert(written->length == -45.0f);

    ++g_passed; printf("  PASS: tools/call update_camera_keyframes\n");
}

int main() {
    g_reader.set(42);

    mmp::CameraKeyFrameData camKf{};
    camKf.frame_no = 42;
    camKf.length = -45.0f;
    camKf.view_angle = 30;
    camKf.is_perspective = 1;
    g_cam_accessor.seed(42, camKf);

    McpServer server(TEST_PORT);
    server.registerTool(std::make_unique<PingTool>());
    server.registerTool(std::make_unique<PingTool>());  // 二重登録テスト用
    server.registerTool(std::make_unique<GetFrameTool>(&g_reader));
    server.registerTool(std::make_unique<SetFrameTool>(&g_writer));
    server.registerTool(std::make_unique<GetCameraKeyframesTool>(&g_cam_accessor));
    server.registerTool(std::make_unique<CreateCameraKeyframesTool>(&g_cam_accessor));
    server.registerTool(std::make_unique<UpdateCameraKeyframesTool>(&g_cam_accessor));
    server.registerTool(std::make_unique<DeleteCameraKeyframesTool>(&g_cam_accessor));
    server.start();
    for (int i = 0; i < 50; ++i) {
        auto r = cli.Get("/");
        if (r && r->status == 200) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    printf("Running MCP tests...\n");

    // initializeはヘッダ不要（最初に実行）
    test_initialize();
    test_initialized_notification();

    // セキュリティ検証テスト
    test_origin_header_rejected();

    // ヘッダ検証テスト（initialize後）
    test_missing_protocol_version_header();
    test_wrong_protocol_version_header();
    test_unknown_notification();

    // HTTPメソッド検証
    test_get_mcp_returns_405();

    // 以降は正しいヘッダ付きでテスト
    test_tools_list();
    test_tools_call_ping();
    test_tools_call_unknown();
    test_method_not_found();
    test_invalid_params();
    test_parse_error();
    test_invalid_request();
    test_tools_call_arguments_must_be_object();
    test_jsonrpc_field_required();
    test_tools_call_get_frame();
    test_tools_call_set_frame();
    test_tools_call_set_frame_negative();
    test_tools_call_get_camera_keyframes();
    test_tools_call_update_camera_keyframes();

    server.stop();

    printf("All %d tests passed.\n", g_passed);
    return 0;
}
