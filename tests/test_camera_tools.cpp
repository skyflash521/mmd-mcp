#include "tests/mock_camera.h"
#include "tools/camera/camera_tool.h"
#include <nlohmann/json.hpp>
#include <cassert>
#include <cstdio>
#include <cstring>

using json = nlohmann::json;

static int g_passed = 0;
static MockCameraAccessor g_accessor;

static mmp::CameraKeyFrameData makeSampleKeyframe(int frame) {
    mmp::CameraKeyFrameData kf{};
    kf.frame_no = frame;
    kf.xyz = {1.0f, 2.0f, 3.0f};
    kf.rxyz = {0.1f, 0.2f, 0.3f};
    kf.length = -45.0f;
    kf.view_angle = 30;
    kf.is_perspective = 1;
    kf.is_selected = 0;
    kf.looking_model_index = -1;
    kf.looking_bone_index = -1;
    for (int i = 0; i < 6; ++i) {
        kf.hokan1_x[i] = 20;
        kf.hokan1_y[i] = 20;
        kf.hokan2_x[i] = 107;
        kf.hokan2_y[i] = 107;
    }
    return kf;
}

static json makeFullKeyframeJson(int frame) {
    return {
        {"frame", frame},
        {"position", {{"x", 10.0}, {"y", 20.0}, {"z", 30.0}}},
        {"rotation", {{"x", 0.5}, {"y", 0.6}, {"z", 0.7}}},
        {"distance", -50.0},
        {"fov", 45},
        {"perspective", false},
        {"interpolation", {
            {"x", {{"x1", 20}, {"y1", 20}, {"x2", 107}, {"y2", 107}}},
            {"y", {{"x1", 20}, {"y1", 20}, {"x2", 107}, {"y2", 107}}},
            {"z", {{"x1", 20}, {"y1", 20}, {"x2", 107}, {"y2", 107}}},
            {"rotation", {{"x1", 20}, {"y1", 20}, {"x2", 107}, {"y2", 107}}},
            {"distance", {{"x1", 20}, {"y1", 20}, {"x2", 107}, {"y2", 107}}},
            {"fov", {{"x1", 20}, {"y1", 20}, {"x2", 107}, {"y2", 107}}}
        }}
    };
}

// --- get_camera_keyframes tests ---

static void test_get_single_frame() {
    MockCameraAccessor accessor;
    accessor.seed(42, makeSampleKeyframe(42));
    GetCameraKeyframesTool tool(&accessor);

    auto result = tool.execute(json({{"frames", "42"}}));
    assert(result["isError"] == false);

    auto data = json::parse(result["content"][0]["text"].get<std::string>());
    assert(data["keyframes"].size() == 1);
    auto& kf = data["keyframes"][0];
    assert(kf["frame"] == 42);
    assert(kf["position"]["x"] == 1.0f);
    assert(kf["distance"] == -45.0f);
    assert(kf["fov"] == 30);
    assert(kf["perspective"] == true);
    assert(kf["interpolation"]["x"]["x1"] == 20);

    ++g_passed; printf("  PASS: get single frame\n");
}

static void test_get_range() {
    MockCameraAccessor accessor;
    for (int i = 1; i <= 3; ++i)
        accessor.seed(i, makeSampleKeyframe(i));

    GetCameraKeyframesTool tool(&accessor);
    auto result = tool.execute(json({{"frames", "1-3"}}));
    assert(result["isError"] == false);

    auto data = json::parse(result["content"][0]["text"].get<std::string>());
    assert(data["keyframes"].size() == 3);

    ++g_passed; printf("  PASS: get range\n");
}

static void test_get_no_frames_param() {
    MockCameraAccessor accessor;
    accessor.seed(0, makeSampleKeyframe(0));
    accessor.seed(10, makeSampleKeyframe(10));
    accessor.seed(20, makeSampleKeyframe(20));
    GetCameraKeyframesTool tool(&accessor);

    auto result = tool.execute(json::object());
    assert(result["isError"] == false);
    auto data = json::parse(result["content"][0]["text"].get<std::string>());
    assert(data["keyframes"].size() == 3);

    ++g_passed; printf("  PASS: get no frames param (all keyframes)\n");
}

static void test_get_null_accessor() {
    GetCameraKeyframesTool tool(nullptr);
    auto result = tool.execute(json({{"frames", "0"}}));
    assert(result["isError"] == true);

    ++g_passed; printf("  PASS: get null accessor\n");
}

static void test_get_invalid_format() {
    MockCameraAccessor accessor;
    GetCameraKeyframesTool tool(&accessor);
    auto result = tool.execute(json({{"frames", "abc"}}));
    assert(result["isError"] == true);

    ++g_passed; printf("  PASS: get invalid format\n");
}

// --- create_camera_keyframes tests ---

static void test_create_success() {
    MockCameraAccessor accessor;
    CreateCameraKeyframesTool tool(&accessor);

    auto result = tool.execute(json({{"keyframes", json::array({makeFullKeyframeJson(10)})}}));
    assert(result["isError"] == false);

    auto* kf = accessor.get(10);
    assert(kf != nullptr);
    assert(kf->xyz.x == 10.0f);
    assert(kf->xyz.y == 20.0f);
    assert(kf->xyz.z == 30.0f);
    assert(kf->length == -50.0f);
    assert(kf->view_angle == 45);
    assert(kf->is_perspective == 0);

    ++g_passed; printf("  PASS: create success\n");
}

static void test_create_already_exists() {
    MockCameraAccessor accessor;
    accessor.seed(10, makeSampleKeyframe(10));
    CreateCameraKeyframesTool tool(&accessor);

    auto result = tool.execute(json({{"keyframes", json::array({makeFullKeyframeJson(10)})}}));
    assert(result["isError"] == true);

    ++g_passed; printf("  PASS: create already exists\n");
}

static void test_create_missing_fields() {
    MockCameraAccessor accessor;
    CreateCameraKeyframesTool tool(&accessor);

    // frame only
    auto result = tool.execute(json({{"keyframes", json::array({{{"frame", 5}}})}}));
    assert(result["isError"] == true);

    ++g_passed; printf("  PASS: create missing fields\n");
}

static void test_create_multiple() {
    MockCameraAccessor accessor;
    CreateCameraKeyframesTool tool(&accessor);

    json kfs = json::array({makeFullKeyframeJson(1), makeFullKeyframeJson(2), makeFullKeyframeJson(3)});
    auto result = tool.execute(json({{"keyframes", kfs}}));
    assert(result["isError"] == false);
    assert(accessor.size() == 3);

    ++g_passed; printf("  PASS: create multiple\n");
}

static void test_create_null_accessor() {
    CreateCameraKeyframesTool tool(nullptr);
    auto result = tool.execute(json({{"keyframes", json::array({makeFullKeyframeJson(0)})}}));
    assert(result["isError"] == true);

    ++g_passed; printf("  PASS: create null accessor\n");
}

// --- update_camera_keyframes tests ---

static void test_update_partial() {
    MockCameraAccessor accessor;
    accessor.seed(5, makeSampleKeyframe(5));
    UpdateCameraKeyframesTool tool(&accessor);

    auto result = tool.execute(json({{"keyframes", json::array({{{"frame", 5}, {"distance", -100.0}}})}}));
    assert(result["isError"] == false);

    auto* kf = accessor.get(5);
    assert(kf != nullptr);
    assert(kf->length == -100.0f);
    // 他のフィールドは変更なし
    assert(kf->xyz.x == 1.0f);
    assert(kf->view_angle == 30);

    ++g_passed; printf("  PASS: update partial\n");
}

static void test_update_not_exists() {
    MockCameraAccessor accessor;
    UpdateCameraKeyframesTool tool(&accessor);

    auto result = tool.execute(json({{"keyframes", json::array({{{"frame", 99}, {"fov", 60}}})}}));
    assert(result["isError"] == true);

    ++g_passed; printf("  PASS: update not exists\n");
}

static void test_update_multiple() {
    MockCameraAccessor accessor;
    for (int i = 0; i < 3; ++i) accessor.seed(i, makeSampleKeyframe(i));
    UpdateCameraKeyframesTool tool(&accessor);

    json kfs = json::array({
        {{"frame", 0}, {"fov", 60}},
        {{"frame", 1}, {"fov", 90}},
        {{"frame", 2}, {"fov", 120}}
    });

    auto result = tool.execute(json({{"keyframes", kfs}}));
    assert(result["isError"] == false);
    assert(accessor.get(0)->view_angle == 60);
    assert(accessor.get(1)->view_angle == 90);
    assert(accessor.get(2)->view_angle == 120);

    ++g_passed; printf("  PASS: update multiple\n");
}

static void test_update_null_accessor() {
    UpdateCameraKeyframesTool tool(nullptr);
    auto result = tool.execute(json({{"keyframes", json::array({{{"frame", 0}}})}}));
    assert(result["isError"] == true);

    ++g_passed; printf("  PASS: update null accessor\n");
}

// --- delete_camera_keyframes tests ---

static void test_delete_success() {
    MockCameraAccessor accessor;
    accessor.seed(10, makeSampleKeyframe(10));
    accessor.seed(20, makeSampleKeyframe(20));
    DeleteCameraKeyframesTool tool(&accessor);

    auto result = tool.execute(json({{"frames", "10"}}));
    assert(result["isError"] == false);
    assert(accessor.get(10) == nullptr);
    assert(accessor.get(20) != nullptr);
    assert(accessor.size() == 1);

    ++g_passed; printf("  PASS: delete success\n");
}

static void test_delete_not_exists() {
    MockCameraAccessor accessor;
    DeleteCameraKeyframesTool tool(&accessor);

    auto result = tool.execute(json({{"frames", "99"}}));
    assert(result["isError"] == true);

    ++g_passed; printf("  PASS: delete not exists\n");
}

static void test_delete_range() {
    MockCameraAccessor accessor;
    for (int i = 1; i <= 5; ++i) accessor.seed(i, makeSampleKeyframe(i));
    DeleteCameraKeyframesTool tool(&accessor);

    auto result = tool.execute(json({{"frames", "2-4"}}));
    assert(result["isError"] == false);
    assert(accessor.size() == 2);
    assert(accessor.get(1) != nullptr);
    assert(accessor.get(5) != nullptr);
    assert(accessor.get(2) == nullptr);

    ++g_passed; printf("  PASS: delete range\n");
}

static void test_delete_frame0_rejected() {
    // フレーム0のみ指定 → エラーで通知
    g_accessor.reset({0, 10, 20});
    DeleteCameraKeyframesTool tool(&g_accessor);
    auto result = tool.execute(json({{"frames", "0"}}));
    assert(result["isError"] == true);
    std::string text = result["content"][0]["text"].get<std::string>();
    assert(text.find("Frame 0") != std::string::npos);
    // フレーム0が残っていることを確認
    assert(g_accessor.count() == 3);

    ++g_passed; printf("  PASS: delete frame 0 rejected\n");
}

static void test_delete_range_with_frame0() {
    // フレーム0を含む範囲 → 0以外は削除され、0はwarningで通知
    g_accessor.reset({0, 10, 20});
    DeleteCameraKeyframesTool tool(&g_accessor);
    auto result = tool.execute(json({{"frames", "0-10"}}));
    assert(result["isError"] == false);
    std::string text = result["content"][0]["text"].get<std::string>();
    assert(text.find("Deleted 1") != std::string::npos);
    assert(text.find("Frame 0") != std::string::npos);
    // フレーム0と20が残る
    assert(g_accessor.count() == 2);

    ++g_passed; printf("  PASS: delete range with frame 0\n");
}

static void test_delete_null_accessor() {
    DeleteCameraKeyframesTool tool(nullptr);
    auto result = tool.execute(json({{"frames", "0"}}));
    assert(result["isError"] == true);

    ++g_passed; printf("  PASS: delete null accessor\n");
}

// --- 部分成功+警告テスト ---

static void test_create_partial_success() {
    // valid + invalid（補間値範囲外）混在 → 成功1件 + 警告
    MockCameraAccessor accessor;
    CreateCameraKeyframesTool tool(&accessor);

    json validKf = makeFullKeyframeJson(10);
    json invalidKf = makeFullKeyframeJson(20);
    invalidKf["interpolation"]["x"]["x1"] = 200;  // 範囲外

    auto result = tool.execute(json({{"keyframes", json::array({validKf, invalidKf})}}));
    assert(result["isError"] == false);  // 1件は成功

    std::string text = result["content"][0]["text"].get<std::string>();
    assert(text.find("Created 1") != std::string::npos);
    assert(text.find("warnings") != std::string::npos);
    assert(text.find("Interpolation") != std::string::npos);

    // valid のみ作成され、invalid は作成されていない
    assert(accessor.get(10) != nullptr);
    assert(accessor.get(20) == nullptr);

    ++g_passed; printf("  PASS: create partial success with warnings\n");
}

static void test_update_partial_success() {
    // valid + invalid（補間値範囲外）混在 → 成功1件 + 警告
    MockCameraAccessor accessor;
    accessor.seed(10, makeSampleKeyframe(10));
    accessor.seed(20, makeSampleKeyframe(20));
    UpdateCameraKeyframesTool tool(&accessor);

    json kfs = json::array({
        {{"frame", 10}, {"fov", 60}},
        {{"frame", 20}, {"interpolation", {{"x", {{"x1", -1}}}}}}  // 範囲外
    });

    auto result = tool.execute(json({{"keyframes", kfs}}));
    assert(result["isError"] == false);

    std::string text = result["content"][0]["text"].get<std::string>();
    assert(text.find("Updated 1") != std::string::npos);
    assert(text.find("warnings") != std::string::npos);

    // frame 10 は更新済み、frame 20 は元の値のまま
    assert(accessor.get(10)->view_angle == 60);
    assert(accessor.get(20)->view_angle == 30);  // 元の値

    ++g_passed; printf("  PASS: update partial success with warnings\n");
}

static void test_create_all_invalid() {
    // 全て無効 → isError=true
    MockCameraAccessor accessor;
    CreateCameraKeyframesTool tool(&accessor);

    json invalidKf = makeFullKeyframeJson(10);
    invalidKf["interpolation"]["x"]["x1"] = 200;  // 範囲外

    auto result = tool.execute(json({{"keyframes", json::array({invalidKf})}}));
    assert(result["isError"] == true);
    assert(accessor.size() == 0);

    ++g_passed; printf("  PASS: create all invalid\n");
}

// --- 境界値テスト ---

static void test_create_frame_max_boundary() {
    // frame 9999 (MAX_CAMERA_FRAMES-1) は許可
    MockCameraAccessor accessor;
    CreateCameraKeyframesTool tool(&accessor);
    auto result = tool.execute(json({{"keyframes", json::array({makeFullKeyframeJson(9999)})}}));
    assert(result["isError"] == false);
    assert(accessor.get(9999) != nullptr);

    ++g_passed; printf("  PASS: create frame 9999 (max boundary)\n");
}

static void test_create_frame_over_max() {
    // frame 10000 (MAX_CAMERA_FRAMES) は拒否
    MockCameraAccessor accessor;
    CreateCameraKeyframesTool tool(&accessor);
    auto result = tool.execute(json({{"keyframes", json::array({makeFullKeyframeJson(10000)})}}));
    assert(result["isError"] == true);
    assert(accessor.size() == 0);

    ++g_passed; printf("  PASS: create frame 10000 (over max rejected)\n");
}

static void test_create_frame_negative() {
    // frame -1 は拒否
    MockCameraAccessor accessor;
    CreateCameraKeyframesTool tool(&accessor);
    auto result = tool.execute(json({{"keyframes", json::array({makeFullKeyframeJson(-1)})}}));
    assert(result["isError"] == true);
    assert(accessor.size() == 0);

    ++g_passed; printf("  PASS: create frame -1 (negative rejected)\n");
}

static void test_create_duplicate_frames_in_request() {
    // 同一リクエスト内で同じフレーム番号が重複 → 1件目成功、2件目はAlreadyExists
    MockCameraAccessor accessor;
    CreateCameraKeyframesTool tool(&accessor);
    json kfs = json::array({makeFullKeyframeJson(5), makeFullKeyframeJson(5)});
    auto result = tool.execute(json({{"keyframes", kfs}}));
    // 1件は成功するので isError=false（部分成功）
    assert(result["isError"] == false);
    std::string text = result["content"][0]["text"].get<std::string>();
    assert(text.find("Created 1") != std::string::npos);
    assert(text.find("already exists") != std::string::npos);

    ++g_passed; printf("  PASS: create duplicate frames in request\n");
}

static void test_update_frame_over_max() {
    // frame 10000 は範囲外で拒否
    MockCameraAccessor accessor;
    UpdateCameraKeyframesTool tool(&accessor);
    auto result = tool.execute(json({{"keyframes", json::array({{{"frame", 10000}, {"fov", 60}}})}}));
    assert(result["isError"] == true);

    ++g_passed; printf("  PASS: update frame 10000 (over max rejected)\n");
}

int main() {
    printf("Running camera tool tests...\n");

    // Read
    test_get_single_frame();
    test_get_range();
    test_get_no_frames_param();
    test_get_null_accessor();
    test_get_invalid_format();

    // Create
    test_create_success();
    test_create_already_exists();
    test_create_missing_fields();
    test_create_multiple();
    test_create_null_accessor();

    // Update
    test_update_partial();
    test_update_not_exists();
    test_update_multiple();
    test_update_null_accessor();

    // Delete
    test_delete_success();
    test_delete_not_exists();
    test_delete_range();
    test_delete_frame0_rejected();
    test_delete_range_with_frame0();
    test_delete_null_accessor();

    // 部分成功+警告
    test_create_partial_success();
    test_update_partial_success();
    test_create_all_invalid();

    // 境界値
    test_create_frame_max_boundary();
    test_create_frame_over_max();
    test_create_frame_negative();
    test_create_duplicate_frames_in_request();
    test_update_frame_over_max();

    printf("All %d camera tool tests passed.\n", g_passed);
    return 0;
}
