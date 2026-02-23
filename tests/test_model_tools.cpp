#include "tests/test_common_setup.h"
#include "tests/mock_model.h"
#include "tools/model/model_tool.h"
#include <nlohmann/json.hpp>
#include <cassert>
#include <cstdio>

using json = nlohmann::json;

static int g_passed = 0;

// --- list_models ---

static void test_list_models() {
    MockModelAccessor accessor;
    accessor.seed(0, {"Miku", "Miku_EN", "", "", 150, 30, 10, true});
    accessor.seed(2, {"Rin", "Rin_EN", "", "", 120, 20, 8, false});
    ListModelsTool tool(&accessor);

    auto result = tool.execute(json::object());
    assert(result["isError"] == false);

    auto data = json::parse(result["content"][0]["text"].get<std::string>());
    assert(data["models"].size() == 2);
    assert(data["models"][0]["index"] == 0);
    assert(data["models"][0]["name_jp"] == "Miku");
    assert(data["models"][0]["bone_count"] == 150);
    assert(data["models"][0]["is_visible"] == true);
    assert(data["models"][1]["index"] == 2);
    assert(data["models"][1]["name_jp"] == "Rin");
    assert(data["models"][1]["is_visible"] == false);

    ++g_passed; printf("  PASS: list_models\n");
}

static void test_list_models_empty() {
    MockModelAccessor accessor;
    ListModelsTool tool(&accessor);

    auto result = tool.execute(json::object());
    assert(result["isError"] == false);

    auto data = json::parse(result["content"][0]["text"].get<std::string>());
    assert(data["models"].empty());

    ++g_passed; printf("  PASS: list_models empty\n");
}

static void test_list_models_null_accessor() {
    ListModelsTool tool(nullptr);

    auto result = tool.execute(json::object());
    assert(result["isError"] == true);

    ++g_passed; printf("  PASS: list_models null accessor\n");
}

// --- get_model_info ---

static void test_get_model_info() {
    MockModelAccessor accessor;
    ModelInfo info{"Miku", "Miku_EN", "Test comment", "C:\\Models\\miku.pmx", 150, 30, 10, true};
    std::vector<BoneInfo> bones = {{"center", "center_en"}, {"upper", "upper_en"}};
    std::vector<MorphBasicInfo> morphs = {
        {"blink", "blink_en", 2, 1},
        {"smile", "smile_en", 3, 1},
        {"body_red", "body_red_en", 4, 8}
    };
    accessor.seed(0, info, bones, morphs);
    GetModelInfoTool tool(&accessor);

    auto result = tool.execute({{"index", 0}});
    assert(result["isError"] == false);

    auto data = json::parse(result["content"][0]["text"].get<std::string>());
    assert(data["index"] == 0);
    assert(data["name_jp"] == "Miku");
    assert(data["name_en"] == "Miku_EN");
    assert(data["comment_jp"] == "Test comment");
    assert(data["file_path"] == "C:\\Models\\miku.pmx");
    assert(data["bone_count"] == 150);
    assert(data["morph_count"] == 30);
    assert(data["ik_count"] == 10);
    assert(data["is_visible"] == true);
    assert(data["bones"].size() == 2);
    assert(data["bones"][0]["index"] == 0);
    assert(data["bones"][0]["name_jp"] == "center");
    assert(data["bones"][1]["name_jp"] == "upper");
    assert(data["morphs"].size() == 3);
    assert(data["morphs"][0]["index"] == 0);
    assert(data["morphs"][0]["name_jp"] == "blink");
    assert(data["morphs"][0]["panel"] == 2);
    assert(data["morphs"][0]["type"] == 1);
    assert(data["morphs"][1]["name_jp"] == "smile");
    assert(data["morphs"][2]["name_jp"] == "body_red");
    assert(data["morphs"][2]["type"] == 8);
    assert(data["morph_source"] == "pmx");

    ++g_passed; printf("  PASS: get_model_info\n");
}

static void test_get_model_info_not_found() {
    MockModelAccessor accessor;
    GetModelInfoTool tool(&accessor);

    auto result = tool.execute({{"index", 99}});
    assert(result["isError"] == true);

    ++g_passed; printf("  PASS: get_model_info not found\n");
}

static void test_get_model_info_missing_arg() {
    MockModelAccessor accessor;
    GetModelInfoTool tool(&accessor);

    auto result = tool.execute(json::object());
    assert(result["isError"] == true);

    ++g_passed; printf("  PASS: get_model_info missing arg\n");
}

static void test_get_model_info_negative_index() {
    MockModelAccessor accessor;
    GetModelInfoTool tool(&accessor);

    auto result = tool.execute({{"index", -1}});
    assert(result["isError"] == true);

    ++g_passed; printf("  PASS: get_model_info negative index\n");
}

static void test_get_model_info_null_accessor() {
    GetModelInfoTool tool(nullptr);

    auto result = tool.execute({{"index", 0}});
    assert(result["isError"] == true);

    ++g_passed; printf("  PASS: get_model_info null accessor\n");
}

static void test_get_model_info_no_bones() {
    MockModelAccessor accessor;
    accessor.seed(0, {"Miku", "Miku_EN", "", "", 0, 0, 0, true});
    GetModelInfoTool tool(&accessor);

    auto result = tool.execute({{"index", 0}});
    assert(result["isError"] == false);

    auto data = json::parse(result["content"][0]["text"].get<std::string>());
    assert(data["bones"].empty());
    assert(data["morphs"].empty());
    assert(data["morph_source"] == "pmx");

    ++g_passed; printf("  PASS: get_model_info no bones\n");
}

static void test_get_model_info_morph_source_unavailable() {
    MockModelAccessor accessor;
    accessor.seed(0, {"Miku", "Miku_EN", "", "", 150, 30, 10, true});
    accessor.setMorphsFail(0);
    GetModelInfoTool tool(&accessor);

    auto result = tool.execute({{"index", 0}});
    assert(result["isError"] == false);

    auto data = json::parse(result["content"][0]["text"].get<std::string>());
    assert(data["morphs"].empty());
    assert(data["morph_source"] == "unavailable");

    ++g_passed; printf("  PASS: get_model_info morph_source unavailable\n");
}

// --- file_modified_after_launch ---

static void test_get_model_info_file_modified() {
    MockModelAccessor accessor;
    accessor.seed(0, {"Miku", "Miku_EN", "", "", 150, 30, 10, true});
    accessor.setMorphsFileModified(0);
    GetModelInfoTool tool(&accessor);

    auto result = tool.execute({{"index", 0}});
    assert(result["isError"] == true);
    auto msg = result["content"][0]["text"].get<std::string>();
    assert(msg.find("modified after MMD was launched") != std::string::npos);
    assert(msg.find("restart MMD") != std::string::npos);

    ++g_passed; printf("  PASS: get_model_info file modified after launch\n");
}

static void test_get_model_info_file_modified_bones_only() {
    MockModelAccessor accessor;
    accessor.seed(0, {"Miku", "Miku_EN", "", "", 150, 30, 10, true},
        {{"center", "center_en"}});
    accessor.setMorphsFileModified(0);
    GetModelInfoTool tool(&accessor);

    // include: ["bones"] の場合、PMXパース不要なのでエラーにならない
    auto result = tool.execute({{"index", 0}, {"include", json::array({"bones"})}});
    assert(result["isError"] == false);

    auto data = json::parse(result["content"][0]["text"].get<std::string>());
    assert(data.contains("bones"));
    assert(!data.contains("morphs"));

    ++g_passed; printf("  PASS: get_model_info file modified but bones only (no error)\n");
}

// --- include filter ---

static MockModelAccessor makeIncludeTestAccessor() {
    MockModelAccessor accessor;
    ModelInfo info{"Miku", "Miku_EN", "comment", "C:\\miku.pmx", 150, 30, 10, true};
    std::vector<BoneInfo> bones = {{"center", "center_en"}};
    std::vector<MorphBasicInfo> morphs = {{"smile", "smile_en", 3, 1}};
    accessor.seed(0, info, bones, morphs);
    return accessor;
}

static void test_get_model_info_include_morphs_only() {
    auto accessor = makeIncludeTestAccessor();
    GetModelInfoTool tool(&accessor);

    auto result = tool.execute({{"index", 0}, {"include", json::array({"morphs"})}});
    assert(result["isError"] == false);

    auto data = json::parse(result["content"][0]["text"].get<std::string>());
    assert(!data.contains("bones"));
    assert(data.contains("morphs"));
    assert(data["morphs"].size() == 1);
    assert(data["morphs"][0]["name_jp"] == "smile");
    assert(data.contains("morph_source"));
    assert(data["morph_source"] == "pmx");

    ++g_passed; printf("  PASS: get_model_info include morphs only\n");
}

static void test_get_model_info_include_bones_only() {
    auto accessor = makeIncludeTestAccessor();
    GetModelInfoTool tool(&accessor);

    auto result = tool.execute({{"index", 0}, {"include", json::array({"bones"})}});
    assert(result["isError"] == false);

    auto data = json::parse(result["content"][0]["text"].get<std::string>());
    assert(data.contains("bones"));
    assert(data["bones"].size() == 1);
    assert(!data.contains("morphs"));
    assert(!data.contains("morph_source"));

    ++g_passed; printf("  PASS: get_model_info include bones only\n");
}

static void test_get_model_info_include_empty() {
    auto accessor = makeIncludeTestAccessor();
    GetModelInfoTool tool(&accessor);

    auto result = tool.execute({{"index", 0}, {"include", json::array()}});
    assert(result["isError"] == false);

    auto data = json::parse(result["content"][0]["text"].get<std::string>());
    assert(!data.contains("bones"));
    assert(!data.contains("morphs"));
    assert(!data.contains("morph_source"));
    assert(data["name_jp"] == "Miku");

    ++g_passed; printf("  PASS: get_model_info include empty\n");
}

static void test_get_model_info_include_both() {
    auto accessor = makeIncludeTestAccessor();
    GetModelInfoTool tool(&accessor);

    auto result = tool.execute({{"index", 0}, {"include", json::array({"bones", "morphs"})}});
    assert(result["isError"] == false);

    auto data = json::parse(result["content"][0]["text"].get<std::string>());
    assert(data.contains("bones"));
    assert(data.contains("morphs"));
    assert(data.contains("morph_source"));

    ++g_passed; printf("  PASS: get_model_info include both\n");
}

static void test_get_model_info_include_unknown() {
    auto accessor = makeIncludeTestAccessor();
    GetModelInfoTool tool(&accessor);

    auto result = tool.execute({{"index", 0}, {"include", json::array({"unknown"})}});
    assert(result["isError"] == true);

    auto msg = result["content"][0]["text"].get<std::string>();
    assert(msg.find("Unknown include value") != std::string::npos);

    ++g_passed; printf("  PASS: get_model_info include unknown value\n");
}

int main() {
    suppressWindowsDialogs();
    printf("Running model tool tests...\n");

    test_list_models();
    test_list_models_empty();
    test_list_models_null_accessor();
    test_get_model_info();
    test_get_model_info_not_found();
    test_get_model_info_missing_arg();
    test_get_model_info_negative_index();
    test_get_model_info_null_accessor();
    test_get_model_info_no_bones();
    test_get_model_info_morph_source_unavailable();
    test_get_model_info_file_modified();
    test_get_model_info_file_modified_bones_only();
    test_get_model_info_include_morphs_only();
    test_get_model_info_include_bones_only();
    test_get_model_info_include_empty();
    test_get_model_info_include_both();
    test_get_model_info_include_unknown();

    printf("All %d tests passed.\n", g_passed);
    return 0;
}
