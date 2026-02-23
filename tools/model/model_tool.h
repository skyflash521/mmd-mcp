#pragma once

#include "tool.h"
#include "tools/model/model_accessor.h"
#include <string>

class ListModelsTool : public ITool {
public:
    explicit ListModelsTool(IModelAccessor* accessor) : accessor_(accessor) {}

    std::string name() const override { return "list_models"; }
    std::string description() const override { return "List all loaded models with basic info"; }

    nlohmann::json inputSchema() const override {
        return {{"type", "object"}, {"properties", nlohmann::json::object()}, {"additionalProperties", false}};
    }

    nlohmann::json execute(const nlohmann::json&) override {
        using json = nlohmann::json;
        if (!accessor_) {
            return {
                {"content", json::array({{{"type", "text"}, {"text", "MMD data not available"}}})},
                {"isError", true}
            };
        }

        auto models = accessor_->listModels();
        json arr = json::array();
        for (auto& [index, info] : models) {
            arr.push_back({
                {"index", index},
                {"name_jp", info.name_jp},
                {"name_en", info.name_en},
                {"bone_count", info.bone_count},
                {"morph_count", info.morph_count},
                {"is_visible", info.is_visible}
            });
        }

        json result = {{"models", arr}};
        return {
            {"content", json::array({{{"type", "text"}, {"text", result.dump()}}})},
            {"isError", false}
        };
    }

private:
    IModelAccessor* accessor_;
};

class GetModelInfoTool : public ITool {
public:
    explicit GetModelInfoTool(IModelAccessor* accessor) : accessor_(accessor) {}

    std::string name() const override { return "get_model_info"; }
    std::string description() const override { return "Get detailed information about a specific model"; }

    nlohmann::json inputSchema() const override {
        return {
            {"type", "object"},
            {"properties", {
                {"index", {{"type", "integer"}, {"description", "Model index (from list_models)"}, {"minimum", 0}}},
                {"include", {
                    {"type", "array"},
                    {"items", {{"type", "string"}, {"enum", {"bones", "morphs"}}}},
                    {"description", "Sections to include in response. Omit for all sections."}
                }}
            }},
            {"required", nlohmann::json::array({"index"})},
            {"additionalProperties", false}
        };
    }

    nlohmann::json execute(const nlohmann::json& args) override {
        using json = nlohmann::json;
        if (!accessor_) {
            return errorResult("MMD data not available");
        }
        if (!args.contains("index") || !args["index"].is_number_integer()) {
            return errorResult("Missing or invalid 'index' argument");
        }

        int index = args["index"].get<int>();
        if (index < 0) {
            return errorResult("Index must be non-negative");
        }

        // include フィルタの解析
        bool includeBones = true;
        bool includeMorphs = true;
        if (args.contains("include")) {
            if (!args["include"].is_array()) {
                return errorResult("'include' must be an array");
            }
            includeBones = false;
            includeMorphs = false;
            for (auto& v : args["include"]) {
                if (!v.is_string()) {
                    return errorResult("'include' array elements must be strings");
                }
                auto s = v.get<std::string>();
                if (s == "bones") includeBones = true;
                else if (s == "morphs") includeMorphs = true;
                else return errorResult("Unknown include value: '" + s + "'. Valid values: bones, morphs");
            }
        }

        ModelInfo info;
        if (!accessor_->getModelInfo(index, info)) {
            return errorResult("Model not found at index " + std::to_string(index));
        }

        json result = {
            {"index", index},
            {"name_jp", info.name_jp},
            {"name_en", info.name_en},
            {"comment_jp", info.comment_jp},
            {"file_path", info.file_path},
            {"bone_count", info.bone_count},
            {"morph_count", info.morph_count},
            {"ik_count", info.ik_count},
            {"is_visible", info.is_visible}
        };

        if (includeBones) {
            auto bones = accessor_->getBones(index);
            json bonesArr = json::array();
            for (int i = 0; i < static_cast<int>(bones.size()); ++i) {
                bonesArr.push_back({
                    {"index", i},
                    {"name_jp", bones[i].name_jp},
                    {"name_en", bones[i].name_en}
                });
            }
            result["bones"] = bonesArr;
        }

        if (includeMorphs) {
            auto [pmxStatus, morphs] = accessor_->getMorphs(index);
            if (pmxStatus == PmxStatus::file_modified_after_launch) {
                return errorResult(
                    "The PMX file for this model was modified after MMD was launched. "
                    "Morph data may be inconsistent. "
                    "Please ask the user to restart MMD and try again.");
            }
            json morphsArr = json::array();
            for (int i = 0; i < static_cast<int>(morphs.size()); ++i) {
                morphsArr.push_back({
                    {"index", i},
                    {"name_jp", morphs[i].name_jp},
                    {"name_en", morphs[i].name_en},
                    {"panel", morphs[i].panel},
                    {"type", morphs[i].type}
                });
            }
            result["morphs"] = morphsArr;
            result["morph_source"] = (pmxStatus == PmxStatus::ok) ? "pmx" : "unavailable";
        }

        return {
            {"content", json::array({{{"type", "text"}, {"text", result.dump()}}})},
            {"isError", false}
        };
    }

private:
    static nlohmann::json errorResult(const std::string& msg) {
        using json = nlohmann::json;
        return {
            {"content", json::array({{{"type", "text"}, {"text", msg}}})},
            {"isError", true}
        };
    }

private:
    IModelAccessor* accessor_;
};
