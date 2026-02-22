#include "tests/test_common_setup.h"
#include "tests/mock_timeline.h"
#include "tools/timeline/timeline_tool.h"
#include <nlohmann/json.hpp>
#include <cassert>
#include <cstdio>

using json = nlohmann::json;

static int g_passed = 0;

static void test_get_current_frame() {
    MockCurrentFrameReader reader;
    reader.set(42);
    GetCurrentFrameTool tool(&reader);

    auto result = tool.execute(json::object());
    assert(result["isError"] == false);
    assert(result["content"][0]["text"] == "42");

    ++g_passed; printf("  PASS: get_current_frame\n");
}

static void test_set_current_frame() {
    MockCurrentFrameWriter writer;
    SetCurrentFrameTool tool(&writer);

    auto result = tool.execute({{"frame", 100}});
    assert(result["isError"] == false);
    assert(result["content"][0]["text"] == "Frame set to 100");
    assert(writer.written() == 100);

    ++g_passed; printf("  PASS: set_current_frame\n");
}

static void test_set_current_frame_missing_arg() {
    MockCurrentFrameWriter writer;
    SetCurrentFrameTool tool(&writer);

    auto result = tool.execute(json::object());
    assert(result["isError"] == true);

    ++g_passed; printf("  PASS: set_current_frame missing arg\n");
}

static void test_set_current_frame_invalid_type() {
    MockCurrentFrameWriter writer;
    SetCurrentFrameTool tool(&writer);

    auto result = tool.execute({{"frame", "not a number"}});
    assert(result["isError"] == true);

    ++g_passed; printf("  PASS: set_current_frame invalid type\n");
}

static void test_get_current_frame_no_provider() {
    GetCurrentFrameTool tool(nullptr);

    auto result = tool.execute(json::object());
    assert(result["isError"] == true);

    ++g_passed; printf("  PASS: get_current_frame no provider\n");
}

static void test_set_current_frame_no_provider() {
    SetCurrentFrameTool tool(nullptr);

    auto result = tool.execute({{"frame", 0}});
    assert(result["isError"] == true);

    ++g_passed; printf("  PASS: set_current_frame no provider\n");
}

static void test_get_current_frame_failure() {
    MockCurrentFrameReader reader;
    reader.set(-1);
    GetCurrentFrameTool tool(&reader);

    auto result = tool.execute(json::object());
    assert(result["isError"] == true);

    ++g_passed; printf("  PASS: get_current_frame failure\n");
}

static void test_set_current_frame_negative() {
    MockCurrentFrameWriter writer;
    SetCurrentFrameTool tool(&writer);

    auto result = tool.execute({{"frame", -5}});
    assert(result["isError"] == true);

    ++g_passed; printf("  PASS: set_current_frame negative\n");
}

int main() {
    suppressWindowsDialogs();
    printf("Running timeline tool tests...\n");

    test_get_current_frame();
    test_set_current_frame();
    test_set_current_frame_missing_arg();
    test_set_current_frame_invalid_type();
    test_get_current_frame_no_provider();
    test_set_current_frame_no_provider();
    test_get_current_frame_failure();
    test_set_current_frame_negative();

    printf("All %d tests passed.\n", g_passed);
    return 0;
}
