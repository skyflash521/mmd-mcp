#include "tests/mock_frame.h"
#include "tools/frame/frame_tool.h"
#include <nlohmann/json.hpp>
#include <cassert>
#include <cstdio>

using json = nlohmann::json;

static int g_passed = 0;

static void test_get_frame() {
    MockFrameReader reader;
    reader.set(42);
    GetFrameTool tool(&reader);

    auto result = tool.execute(json::object());
    assert(result["isError"] == false);
    assert(result["content"][0]["text"] == "42");

    ++g_passed; printf("  PASS: get_frame\n");
}

static void test_set_frame() {
    MockFrameWriter writer;
    SetFrameTool tool(&writer);

    auto result = tool.execute({{"frame", 100}});
    assert(result["isError"] == false);
    assert(result["content"][0]["text"] == "Frame set to 100");
    assert(writer.written() == 100);

    ++g_passed; printf("  PASS: set_frame\n");
}

static void test_set_frame_missing_arg() {
    MockFrameWriter writer;
    SetFrameTool tool(&writer);

    auto result = tool.execute(json::object());
    assert(result["isError"] == true);

    ++g_passed; printf("  PASS: set_frame missing arg\n");
}

static void test_set_frame_invalid_type() {
    MockFrameWriter writer;
    SetFrameTool tool(&writer);

    auto result = tool.execute({{"frame", "not a number"}});
    assert(result["isError"] == true);

    ++g_passed; printf("  PASS: set_frame invalid type\n");
}

static void test_get_frame_no_provider() {
    GetFrameTool tool(nullptr);

    auto result = tool.execute(json::object());
    assert(result["isError"] == true);

    ++g_passed; printf("  PASS: get_frame no provider\n");
}

static void test_set_frame_no_provider() {
    SetFrameTool tool(nullptr);

    auto result = tool.execute({{"frame", 0}});
    assert(result["isError"] == true);

    ++g_passed; printf("  PASS: set_frame no provider\n");
}

static void test_get_frame_failure() {
    MockFrameReader reader;
    reader.set(-1);
    GetFrameTool tool(&reader);

    auto result = tool.execute(json::object());
    assert(result["isError"] == true);

    ++g_passed; printf("  PASS: get_frame failure\n");
}

static void test_set_frame_negative() {
    MockFrameWriter writer;
    SetFrameTool tool(&writer);

    auto result = tool.execute({{"frame", -5}});
    assert(result["isError"] == true);

    ++g_passed; printf("  PASS: set_frame negative\n");
}

int main() {
    printf("Running frame tool tests...\n");

    test_get_frame();
    test_set_frame();
    test_set_frame_missing_arg();
    test_set_frame_invalid_type();
    test_get_frame_no_provider();
    test_set_frame_no_provider();
    test_get_frame_failure();
    test_set_frame_negative();

    printf("All %d tests passed.\n", g_passed);
    return 0;
}
