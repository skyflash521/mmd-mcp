#include "tests/test_common_setup.h"
#include "tests/mock_morph.h"
#include "tools/morph/morph_tool.h"
#include <nlohmann/json.hpp>
#include <cassert>
#include <cstdio>
#include <cmath>

using json = nlohmann::json;

static int g_passed = 0;

// --- get_morph_keyframes ---

static void test_get_single_frame() {
    MockMorphKeyframeAccessor accessor;
    accessor.setMorphCount(0, 3);
    accessor.seed(0, 1, 5, 0.8f);
    GetMorphKeyframesTool tool(&accessor);

    auto result = tool.execute({{"model_index", 0}, {"morph_index", 1}, {"frames", "5"}});
    assert(result["isError"] == false);

    auto data = json::parse(result["content"][0]["text"].get<std::string>());
    assert(data["keyframes"].size() == 1);
    assert(data["keyframes"][0]["frame"] == 5);
    assert(std::fabs(data["keyframes"][0]["value"].get<float>() - 0.8f) < 0.001f);

    ++g_passed; printf("  PASS: get single frame\n");
}

static void test_get_range() {
    MockMorphKeyframeAccessor accessor;
    accessor.setMorphCount(0, 2);
    accessor.seed(0, 0, 0, 0.0f);
    accessor.seed(0, 0, 5, 0.5f);
    accessor.seed(0, 0, 10, 1.0f);
    accessor.seed(0, 0, 20, 0.3f);
    GetMorphKeyframesTool tool(&accessor);

    auto result = tool.execute({{"model_index", 0}, {"morph_index", 0}, {"frames", "0-10"}});
    assert(result["isError"] == false);

    auto data = json::parse(result["content"][0]["text"].get<std::string>());
    assert(data["keyframes"].size() == 3);

    ++g_passed; printf("  PASS: get range\n");
}

static void test_get_no_frames_param() {
    MockMorphKeyframeAccessor accessor;
    accessor.setMorphCount(0, 1);
    accessor.seed(0, 0, 0, 0.0f);
    accessor.seed(0, 0, 10, 0.5f);
    GetMorphKeyframesTool tool(&accessor);

    auto result = tool.execute({{"model_index", 0}, {"morph_index", 0}});
    assert(result["isError"] == false);

    auto data = json::parse(result["content"][0]["text"].get<std::string>());
    assert(data["keyframes"].size() == 2);

    ++g_passed; printf("  PASS: get no frames param (all keyframes)\n");
}

static void test_get_null_accessor() {
    GetMorphKeyframesTool tool(nullptr);

    auto result = tool.execute({{"model_index", 0}, {"morph_index", 0}});
    assert(result["isError"] == true);

    ++g_passed; printf("  PASS: get null accessor\n");
}

static void test_get_invalid_format() {
    MockMorphKeyframeAccessor accessor;
    accessor.setMorphCount(0, 1);
    GetMorphKeyframesTool tool(&accessor);

    auto result = tool.execute({{"model_index", 0}, {"morph_index", 0}, {"frames", "abc"}});
    assert(result["isError"] == true);

    ++g_passed; printf("  PASS: get invalid format\n");
}

static void test_get_model_not_found() {
    MockMorphKeyframeAccessor accessor;
    GetMorphKeyframesTool tool(&accessor);

    auto result = tool.execute({{"model_index", 99}, {"morph_index", 0}});
    assert(result["isError"] == true);

    ++g_passed; printf("  PASS: get model not found\n");
}

static void test_get_morph_out_of_range() {
    MockMorphKeyframeAccessor accessor;
    accessor.setMorphCount(0, 3);
    GetMorphKeyframesTool tool(&accessor);

    auto result = tool.execute({{"model_index", 0}, {"morph_index", 5}});
    assert(result["isError"] == true);

    ++g_passed; printf("  PASS: get morph out of range\n");
}

static void test_get_missing_model_index() {
    MockMorphKeyframeAccessor accessor;
    GetMorphKeyframesTool tool(&accessor);

    auto result = tool.execute({{"morph_index", 0}});
    assert(result["isError"] == true);

    ++g_passed; printf("  PASS: get missing model_index\n");
}

static void test_get_missing_morph_index() {
    MockMorphKeyframeAccessor accessor;
    GetMorphKeyframesTool tool(&accessor);

    auto result = tool.execute({{"model_index", 0}});
    assert(result["isError"] == true);

    ++g_passed; printf("  PASS: get missing morph_index\n");
}

static void test_get_value_correctness() {
    MockMorphKeyframeAccessor accessor;
    accessor.setMorphCount(1, 2);
    accessor.seed(1, 0, 0, 0.0f);
    accessor.seed(1, 0, 30, 1.0f);
    accessor.seed(1, 1, 0, 0.0f);
    accessor.seed(1, 1, 15, 0.75f);
    GetMorphKeyframesTool tool(&accessor);

    // morph 0
    auto r0 = tool.execute({{"model_index", 1}, {"morph_index", 0}, {"frames", "30"}});
    assert(r0["isError"] == false);
    auto d0 = json::parse(r0["content"][0]["text"].get<std::string>());
    assert(d0["keyframes"].size() == 1);
    assert(std::fabs(d0["keyframes"][0]["value"].get<float>() - 1.0f) < 0.001f);

    // morph 1
    auto r1 = tool.execute({{"model_index", 1}, {"morph_index", 1}, {"frames", "15"}});
    assert(r1["isError"] == false);
    auto d1 = json::parse(r1["content"][0]["text"].get<std::string>());
    assert(d1["keyframes"].size() == 1);
    assert(std::fabs(d1["keyframes"][0]["value"].get<float>() - 0.75f) < 0.001f);

    ++g_passed; printf("  PASS: get value correctness\n");
}

int main() {
    suppressWindowsDialogs();
    printf("Running morph tool tests...\n");

    test_get_single_frame();
    test_get_range();
    test_get_no_frames_param();
    test_get_null_accessor();
    test_get_invalid_format();
    test_get_model_not_found();
    test_get_morph_out_of_range();
    test_get_missing_model_index();
    test_get_missing_morph_index();
    test_get_value_correctness();

    printf("All %d morph tool tests passed.\n", g_passed);
    return 0;
}
