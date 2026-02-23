// MMD結合テスト: MMDが起動中・プラグインロード済みの状態で実行する
// localhost:3939 にHTTPリクエストを送り、カメラキーフレームCRUDの動作を検証する

#include "tests/test_common_setup.h"
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <set>
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

// --- tools/list テスト ---

static void test_tools_list(httplib::Client& cli) {
    auto response = mcpCall(cli, "tools/list");
    assert(response.contains("result"));
    auto& tools = response["result"]["tools"];
    assert(tools.is_array());

    // 登録済みツール名を収集
    std::set<std::string> names;
    for (auto& t : tools) {
        assert(t.contains("name"));
        assert(t.contains("description"));
        assert(t.contains("inputSchema"));
        names.insert(t["name"].get<std::string>());
    }

    // 全ツールが登録されていること
    std::vector<std::string> expected = {
        "ping", "get_current_frame", "set_current_frame",
        "get_camera_keyframes", "create_camera_keyframes",
        "update_camera_keyframes", "delete_camera_keyframes",
        "list_models", "get_model_info",
        "get_morph_keyframes", "get_all_morph_keyframes"
    };
    for (auto& e : expected) {
        assert(names.count(e) == 1);
    }

    ++g_passed; printf("  PASS: tools/list (%d tools)\n", (int)tools.size());
}

// --- カメラキーフレームテスト ---

static void test_get_camera_keyframes_frame0(httplib::Client& cli) {
    // フレーム0は常に存在する
    auto result = callTool(cli, "get_camera_keyframes", {{"frames", "0"}});
    assert(result["isError"] == false);

    auto data = json::parse(getToolText(result));
    assert(data["keyframes"].size() == 1);
    auto& kf = data["keyframes"][0];
    assert(kf["frame"] == 0);
    // 必須フィールドの存在確認
    assert(kf.contains("position"));
    assert(kf.contains("rotation"));
    assert(kf.contains("distance"));
    assert(kf.contains("fov"));
    assert(kf.contains("interpolation"));

    ++g_passed; printf("  PASS: get_camera_keyframes frame 0\n");
}

static void test_get_camera_keyframes_range_filter(httplib::Client& cli) {
    int f1 = TEST_FRAME_BASE + 21;
    int f2 = TEST_FRAME_BASE + 25;
    int f3 = TEST_FRAME_BASE + 30;

    // 3つ作成
    json kfs = json::array({
        makeTestKeyframe(f1, 1.0f), makeTestKeyframe(f2, 2.0f), makeTestKeyframe(f3, 3.0f)
    });
    auto createResult = callTool(cli, "create_camera_keyframes", {{"keyframes", kfs}});
    assert(createResult["isError"] == false);

    // 範囲フィルタ: f1-f2 のみ取得（f3は含まれない）
    std::string range = std::to_string(f1) + "-" + std::to_string(f2);
    auto result = callTool(cli, "get_camera_keyframes", {{"frames", range}});
    assert(result["isError"] == false);

    auto data = json::parse(getToolText(result));
    assert(data["keyframes"].size() == 2);
    assert(data["keyframes"][0]["frame"] == f1);
    assert(data["keyframes"][1]["frame"] == f2);

    // クリーンアップ
    std::string cleanRange = std::to_string(f1) + "-" + std::to_string(f3);
    callTool(cli, "delete_camera_keyframes", {{"frames", cleanRange}});

    ++g_passed; printf("  PASS: get_camera_keyframes range filter\n");
}

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

// --- タイムラインテスト ---

static void test_get_current_frame(httplib::Client& cli) {
    auto result = callTool(cli, "get_current_frame");
    assert(result["isError"] == false);

    std::string text = getToolText(result);
    int frame = std::stoi(text);
    assert(frame >= 0);

    ++g_passed; printf("  PASS: get_current_frame (frame=%d)\n", frame);
}

static void test_set_current_frame(httplib::Client& cli) {
    // 現在フレームを取得して保存
    auto before = callTool(cli, "get_current_frame");
    int originalFrame = std::stoi(getToolText(before));

    // フレームを変更
    int testFrame = 123;
    auto result = callTool(cli, "set_current_frame", {{"frame", testFrame}});
    assert(result["isError"] == false);

    // 変更されたことを確認
    auto after = callTool(cli, "get_current_frame");
    assert(std::stoi(getToolText(after)) == testFrame);

    // 元に戻す
    callTool(cli, "set_current_frame", {{"frame", originalFrame}});

    ++g_passed; printf("  PASS: set_current_frame\n");
}

// --- モデル情報テスト ---

static void test_list_models(httplib::Client& cli) {
    auto result = callTool(cli, "list_models");
    assert(result["isError"] == false);

    auto data = json::parse(getToolText(result));
    assert(data.contains("models"));
    assert(data["models"].is_array());
    assert(data["models"].size() >= 1); // 前提条件: モデル1体以上

    // 各モデルに必須フィールドがあること
    auto& m = data["models"][0];
    assert(m.contains("index"));
    assert(m.contains("name_jp"));
    assert(m.contains("bone_count"));
    assert(m.contains("morph_count"));
    assert(m["bone_count"].get<int>() > 0);
    assert(m["morph_count"].get<int>() > 0);

    ++g_passed; printf("  PASS: list_models (count=%d, name=%s)\n",
        (int)data["models"].size(), m["name_jp"].get<std::string>().c_str());
}

static void test_get_model_info_not_found(httplib::Client& cli) {
    // 存在しないインデックスでエラーが返ること
    auto result = callTool(cli, "get_model_info", {{"index", 254}});
    assert(result["isError"] == true);

    ++g_passed; printf("  PASS: get_model_info not found\n");
}

static void test_get_model_info_with_morphs(httplib::Client& cli) {
    // モーフ名が取得できること
    auto result = callTool(cli, "get_model_info",
        {{"index", 0}, {"include", json::array({"morphs"})}});
    assert(result["isError"] == false);

    auto data = json::parse(getToolText(result));
    assert(data.contains("morphs"));
    assert(data["morphs"].is_array());
    assert(data["morphs"].size() > 0);

    // 各モーフにname_jpが存在し、空でないこと
    auto& first = data["morphs"][0];
    assert(first.contains("index"));
    assert(first.contains("name_jp"));
    assert(!first["name_jp"].get<std::string>().empty());

    // include:["morphs"] ではbonesが返らないこと
    assert(!data.contains("bones"));

    ++g_passed; printf("  PASS: get_model_info with morphs (count=%d, first=%s)\n",
        (int)data["morphs"].size(), first["name_jp"].get<std::string>().c_str());
}

static void test_get_model_info_with_bones(httplib::Client& cli) {
    // ボーン名が取得できること
    auto result = callTool(cli, "get_model_info",
        {{"index", 0}, {"include", json::array({"bones"})}});
    assert(result["isError"] == false);

    auto data = json::parse(getToolText(result));
    assert(data.contains("bones"));
    assert(data["bones"].is_array());
    assert(data["bones"].size() > 0);

    auto& first = data["bones"][0];
    assert(first.contains("name_jp"));
    assert(!first["name_jp"].get<std::string>().empty());

    // include:["bones"] ではmorphsが返らないこと
    assert(!data.contains("morphs"));

    ++g_passed; printf("  PASS: get_model_info with bones (count=%d, first=%s)\n",
        (int)data["bones"].size(), first["name_jp"].get<std::string>().c_str());
}

// --- モーフキーフレームテスト ---

static void test_get_morph_keyframes_not_found(httplib::Client& cli) {
    // 存在しないモデルインデックスでエラーが返ること
    auto result = callTool(cli, "get_morph_keyframes", {{"model_index", 254}, {"morph_index", 0}});
    assert(result["isError"] == true);

    ++g_passed; printf("  PASS: get_morph_keyframes model not found\n");
}

static void test_get_morph_keyframes_by_index(httplib::Client& cli) {
    // morph_index=0 でキーフレームが取得できること
    auto result = callTool(cli, "get_morph_keyframes",
        {{"model_index", 0}, {"morph_index", 0}});
    assert(result["isError"] == false);

    auto data = json::parse(getToolText(result));
    assert(data.contains("keyframes"));
    assert(data["keyframes"].is_array());

    ++g_passed; printf("  PASS: get_morph_keyframes by index (keyframes=%d)\n",
        (int)data["keyframes"].size());
}

static void test_get_morph_keyframes_by_name(httplib::Client& cli) {
    // get_model_info から先頭モーフ名を取得
    auto infoResult = callTool(cli, "get_model_info",
        {{"index", 0}, {"include", json::array({"morphs"})}});
    assert(infoResult["isError"] == false);
    auto infoData = json::parse(getToolText(infoResult));
    assert(infoData["morphs"].size() > 0);
    std::string morphName = infoData["morphs"][0]["name_jp"].get<std::string>();

    // その名前でキーフレーム取得
    auto result = callTool(cli, "get_morph_keyframes",
        {{"model_index", 0}, {"morph_name", morphName}});
    assert(result["isError"] == false);

    auto data = json::parse(getToolText(result));
    assert(data.contains("keyframes"));

    ++g_passed; printf("  PASS: get_morph_keyframes by name (\"%s\", keyframes=%d)\n",
        morphName.c_str(), (int)data["keyframes"].size());
}

static void test_get_morph_name_index_equivalence(httplib::Client& cli) {
    // get_model_info から先頭モーフの名前とインデックスを取得
    auto infoResult = callTool(cli, "get_model_info",
        {{"index", 0}, {"include", json::array({"morphs"})}});
    assert(infoResult["isError"] == false);
    auto infoData = json::parse(getToolText(infoResult));
    assert(infoData["morphs"].size() > 0);
    int morphIndex = infoData["morphs"][0]["index"].get<int>();
    std::string morphName = infoData["morphs"][0]["name_jp"].get<std::string>();

    // morph_index で取得
    auto byIndex = callTool(cli, "get_morph_keyframes",
        {{"model_index", 0}, {"morph_index", morphIndex}});
    assert(byIndex["isError"] == false);
    auto dataIndex = json::parse(getToolText(byIndex));

    // morph_name で取得
    auto byName = callTool(cli, "get_morph_keyframes",
        {{"model_index", 0}, {"morph_name", morphName}});
    assert(byName["isError"] == false);
    auto dataName = json::parse(getToolText(byName));

    // キーフレーム配列が一致
    assert(dataIndex["keyframes"].size() == dataName["keyframes"].size());
    for (size_t i = 0; i < dataIndex["keyframes"].size(); ++i) {
        assert(dataIndex["keyframes"][i]["frame"] == dataName["keyframes"][i]["frame"]);
    }

    ++g_passed; printf("  PASS: morph_name/morph_index equivalence (\"%s\"=index %d)\n",
        morphName.c_str(), morphIndex);
}

static void test_get_all_morph_keyframes(httplib::Client& cli) {
    auto result = callTool(cli, "get_all_morph_keyframes", {{"model_index", 0}});
    assert(result["isError"] == false);

    auto data = json::parse(getToolText(result));
    assert(data.contains("morphs"));
    assert(data["morphs"].is_array());

    // 返されたモーフにname_jpが紐付いていること
    if (data["morphs"].size() > 0) {
        auto& first = data["morphs"][0];
        assert(first.contains("morph_index"));
        assert(first.contains("name_jp"));
        assert(first.contains("keyframes"));
        assert(!first["name_jp"].get<std::string>().empty());
    }

    ++g_passed; printf("  PASS: get_all_morph_keyframes (morphs_with_keyframes=%d)\n",
        (int)data["morphs"].size());
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
            printf("FAIL: MMD is not running or plugin not loaded (cannot connect to %s:%d)\n",
                   HOST, PORT);
            return 1;
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

    // ツール一覧テスト
    test_tools_list(cli);

    // カメラ読み取りテスト
    test_get_camera_keyframes_frame0(cli);

    // モデル情報テスト
    test_list_models(cli);
    test_get_model_info_not_found(cli);
    test_get_model_info_with_morphs(cli);
    test_get_model_info_with_bones(cli);

    // モーフキーフレームテスト
    test_get_morph_keyframes_not_found(cli);
    test_get_morph_keyframes_by_index(cli);
    test_get_morph_keyframes_by_name(cli);
    test_get_morph_name_index_equivalence(cli);
    test_get_all_morph_keyframes(cli);

    // タイムラインテスト
    test_get_current_frame(cli);
    test_set_current_frame(cli);

    // テスト前クリーンアップ
    cleanup(cli);

    // Create テスト
    test_create_and_get(cli);
    test_create_already_exists(cli);
    test_get_camera_keyframes_range_filter(cli);

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
    suppressWindowsDialogs();
    return runTests();
}
