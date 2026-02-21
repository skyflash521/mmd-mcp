// MMD結合テスト: MMDが起動中・プラグインロード済みの状態で実行する
// localhost:3939 にHTTPリクエストを送り、カメラキーフレームCRUDの動作を検証する

#include <httplib.h>
#include <nlohmann/json.hpp>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <string>

using json = nlohmann::json;

static int g_passed = 0;
static constexpr const char* HOST = "127.0.0.1";
static constexpr int PORT = 3939;
static constexpr const char* PROTOCOL_VERSION = "2025-11-25";

// テスト用フレーム番号（通常作業と重複しない範囲）
static constexpr int TEST_FRAME_BASE = 9000;

// --- MCP呼び出しヘルパー ---

static json mcpCall(httplib::Client& cli, const std::string& method,
                    const json& params = json::object(), int id = 1) {
    json body = {
        {"jsonrpc", "2.0"},
        {"method", method},
        {"id", id},
        {"params", params}
    };

    httplib::Headers headers = {
        {"MCP-Protocol-Version", PROTOCOL_VERSION}
    };

    auto res = cli.Post("/mcp", headers, body.dump(), "application/json");
    if (!res) {
        fprintf(stderr, "HTTP request failed: %s\n", httplib::to_string(res.error()).c_str());
        return json::object();
    }
    return json::parse(res->body);
}

static json callTool(httplib::Client& cli, const std::string& toolName,
                     const json& args = json::object()) {
    json params = {{"name", toolName}, {"arguments", args}};
    auto response = mcpCall(cli, "tools/call", params);
    if (response.contains("result")) return response["result"];
    fprintf(stderr, "Tool call error: %s\n", response.dump(2).c_str());
    return response;
}

static std::string getToolText(const json& result) {
    return result["content"][0]["text"].get<std::string>();
}

// テスト用キーフレームJSON
static json makeTestKeyframe(int frame, float posX = 10.0f) {
    return {
        {"frame", frame},
        {"position", {{"x", posX}, {"y", 20.0}, {"z", 30.0}}},
        {"rotation", {{"x", 0.5}, {"y", 0.6}, {"z", 0.7}}},
        {"distance", -50.0},
        {"fov", 45},
        {"perspective", false},
        {"interpolation", {
            {"x", {{"x1", 10}, {"y1", 30}, {"x2", 100}, {"y2", 120}}},
            {"y", {{"x1", 20}, {"y1", 20}, {"x2", 107}, {"y2", 107}}},
            {"z", {{"x1", 20}, {"y1", 20}, {"x2", 107}, {"y2", 107}}},
            {"rotation", {{"x1", 20}, {"y1", 20}, {"x2", 107}, {"y2", 107}}},
            {"distance", {{"x1", 20}, {"y1", 20}, {"x2", 107}, {"y2", 107}}},
            {"fov", {{"x1", 20}, {"y1", 20}, {"x2", 107}, {"y2", 107}}}
        }}
    };
}

// クリーンアップ: テスト用フレームを全削除
static void cleanup(httplib::Client& cli) {
    std::string range = std::to_string(TEST_FRAME_BASE) + "-" +
                        std::to_string(TEST_FRAME_BASE + 99);
    callTool(cli, "delete_camera_keyframes", {{"frames", range}});
}

// 浮動小数点比較
static bool approxEqual(double a, double b, double eps = 0.001) {
    return std::fabs(a - b) < eps;
}

// --- テスト ---

static void test_create_and_get(httplib::Client& cli) {
    int frame = TEST_FRAME_BASE + 1;

    // Create
    auto result = callTool(cli, "create_camera_keyframes",
        {{"keyframes", json::array({makeTestKeyframe(frame)})}});
    assert(result["isError"] == false);

    // Get して値一致確認
    result = callTool(cli, "get_camera_keyframes",
        {{"frames", std::to_string(frame)}});
    assert(result["isError"] == false);

    auto data = json::parse(getToolText(result));
    assert(data["keyframes"].size() == 1);
    auto& kf = data["keyframes"][0];
    assert(kf["frame"] == frame);
    assert(approxEqual(kf["position"]["x"].get<double>(), 10.0));
    assert(approxEqual(kf["position"]["y"].get<double>(), 20.0));
    assert(approxEqual(kf["position"]["z"].get<double>(), 30.0));
    assert(approxEqual(kf["rotation"]["x"].get<double>(), 0.5));
    assert(approxEqual(kf["rotation"]["y"].get<double>(), 0.6));
    assert(approxEqual(kf["rotation"]["z"].get<double>(), 0.7));
    assert(approxEqual(kf["distance"].get<double>(), -50.0));
    assert(kf["fov"] == 45);
    assert(kf["perspective"] == false);

    // 補間曲線
    assert(kf["interpolation"]["x"]["x1"] == 10);
    assert(kf["interpolation"]["x"]["y1"] == 30);
    assert(kf["interpolation"]["x"]["x2"] == 100);
    assert(kf["interpolation"]["x"]["y2"] == 120);
    assert(kf["interpolation"]["y"]["x1"] == 20);

    // クリーンアップ
    callTool(cli, "delete_camera_keyframes",
        {{"frames", std::to_string(frame)}});

    ++g_passed; printf("  PASS: create and get\n");
}

static void test_create_already_exists(httplib::Client& cli) {
    int frame = TEST_FRAME_BASE + 2;

    // 1回目: 成功
    auto result = callTool(cli, "create_camera_keyframes",
        {{"keyframes", json::array({makeTestKeyframe(frame)})}});
    assert(result["isError"] == false);

    // 2回目: エラー
    result = callTool(cli, "create_camera_keyframes",
        {{"keyframes", json::array({makeTestKeyframe(frame)})}});
    assert(result["isError"] == true);

    // クリーンアップ
    callTool(cli, "delete_camera_keyframes",
        {{"frames", std::to_string(frame)}});

    ++g_passed; printf("  PASS: create already exists\n");
}

static void test_update_partial(httplib::Client& cli) {
    int frame = TEST_FRAME_BASE + 3;

    // Create
    callTool(cli, "create_camera_keyframes",
        {{"keyframes", json::array({makeTestKeyframe(frame)})}});

    // Update: distance と fov のみ変更
    auto result = callTool(cli, "update_camera_keyframes",
        {{"keyframes", json::array({
            {{"frame", frame}, {"distance", -100.0}, {"fov", 60}}
        })}});
    assert(result["isError"] == false);

    // Get して確認
    result = callTool(cli, "get_camera_keyframes",
        {{"frames", std::to_string(frame)}});
    auto data = json::parse(getToolText(result));
    auto& kf = data["keyframes"][0];

    // 変更したフィールド
    assert(approxEqual(kf["distance"].get<double>(), -100.0));
    assert(kf["fov"] == 60);

    // 変更していないフィールド（元の値を維持）
    assert(approxEqual(kf["position"]["x"].get<double>(), 10.0));
    assert(approxEqual(kf["rotation"]["x"].get<double>(), 0.5));
    assert(kf["perspective"] == false);
    assert(kf["interpolation"]["x"]["x1"] == 10);

    // クリーンアップ
    callTool(cli, "delete_camera_keyframes",
        {{"frames", std::to_string(frame)}});

    ++g_passed; printf("  PASS: update partial\n");
}

static void test_update_not_exists(httplib::Client& cli) {
    int frame = TEST_FRAME_BASE + 4;

    auto result = callTool(cli, "update_camera_keyframes",
        {{"keyframes", json::array({
            {{"frame", frame}, {"fov", 60}}
        })}});
    assert(result["isError"] == true);

    ++g_passed; printf("  PASS: update not exists\n");
}

static void test_delete_success(httplib::Client& cli) {
    int frame = TEST_FRAME_BASE + 5;

    // Create → Delete
    callTool(cli, "create_camera_keyframes",
        {{"keyframes", json::array({makeTestKeyframe(frame)})}});

    auto result = callTool(cli, "delete_camera_keyframes",
        {{"frames", std::to_string(frame)}});
    assert(result["isError"] == false);

    // Get で存在しないこと確認
    result = callTool(cli, "get_camera_keyframes",
        {{"frames", std::to_string(frame)}});
    auto data = json::parse(getToolText(result));
    assert(data["keyframes"].size() == 0);

    ++g_passed; printf("  PASS: delete success\n");
}

static void test_delete_range(httplib::Client& cli) {
    int f1 = TEST_FRAME_BASE + 11;
    int f2 = TEST_FRAME_BASE + 12;
    int f3 = TEST_FRAME_BASE + 13;

    // 3つ作成
    json kfs = json::array({
        makeTestKeyframe(f1), makeTestKeyframe(f2), makeTestKeyframe(f3)
    });
    callTool(cli, "create_camera_keyframes", {{"keyframes", kfs}});

    // 範囲削除
    std::string range = std::to_string(f1) + "-" + std::to_string(f3);
    auto result = callTool(cli, "delete_camera_keyframes", {{"frames", range}});
    assert(result["isError"] == false);
    std::string text = getToolText(result);
    assert(text.find("Deleted 3") != std::string::npos);

    // 全て存在しないこと確認
    result = callTool(cli, "get_camera_keyframes", {{"frames", range}});
    auto data = json::parse(getToolText(result));
    assert(data["keyframes"].size() == 0);

    ++g_passed; printf("  PASS: delete range\n");
}

static void test_delete_frame0(httplib::Client& cli) {
    auto result = callTool(cli, "delete_camera_keyframes", {{"frames", "0"}});
    assert(result["isError"] == true);
    std::string text = getToolText(result);
    assert(text.find("Frame 0") != std::string::npos);

    ++g_passed; printf("  PASS: delete frame 0 rejected\n");
}

static void test_delete_not_exists(httplib::Client& cli) {
    int frame = TEST_FRAME_BASE + 99;
    auto result = callTool(cli, "delete_camera_keyframes",
        {{"frames", std::to_string(frame)}});
    assert(result["isError"] == true);

    ++g_passed; printf("  PASS: delete not exists\n");
}

static int runTests() {
    httplib::Client cli(HOST, PORT);
    cli.set_connection_timeout(3);
    cli.set_read_timeout(10);

    // 接続確認（initialize）
    {
        json body = {
            {"jsonrpc", "2.0"},
            {"method", "initialize"},
            {"id", 0},
            {"params", {{"protocolVersion", PROTOCOL_VERSION},
                        {"capabilities", json::object()},
                        {"clientInfo", {{"name", "test_integration"}, {"version", "1.0"}}}}}
        };
        auto res = cli.Post("/mcp", body.dump(), "application/json");
        if (!res) {
            printf("MMD is not running or plugin not loaded (cannot connect to %s:%d)\n",
                   HOST, PORT);
            printf("Skipping integration tests.\n");
            return 0;
        }
        auto response = json::parse(res->body);
        if (!response.contains("result") ||
            !response["result"].contains("serverInfo")) {
            printf("Unexpected initialize response: %s\n", response.dump(2).c_str());
            return 1;
        }
    }

    // ping で確認
    {
        auto result = callTool(cli, "ping");
        if (result["isError"] != false) {
            printf("Ping failed. Skipping integration tests.\n");
            return 0;
        }
    }

    printf("Running integration tests (connected to MMD at %s:%d)...\n", HOST, PORT);

    // テスト前クリーンアップ
    cleanup(cli);

    // Create テスト
    test_create_and_get(cli);
    test_create_already_exists(cli);

    // Update テスト
    test_update_partial(cli);
    test_update_not_exists(cli);

    // Delete テスト
    test_delete_success(cli);
    test_delete_range(cli);
    test_delete_frame0(cli);
    test_delete_not_exists(cli);

    // 最終クリーンアップ
    cleanup(cli);

    printf("All %d integration tests passed.\n", g_passed);
    return 0;
}

int main() {
    return runTests();
}
