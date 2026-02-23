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
                {"index", {{"type", "integer"}, {"description", "Model index (from list_models)"}, {"minimum", 0}}}
            }},
            {"required", nlohmann::json::array({"index"})},
            {"additionalProperties", false}
        };
    }

    nlohmann::json execute(const nlohmann::json& args) override {
        using json = nlohmann::json;
        if (!accessor_) {
            return {
                {"content", json::array({{{"type", "text"}, {"text", "MMD data not available"}}})},
                {"isError", true}
            };
        }
        if (!args.contains("index") || !args["index"].is_number_integer()) {
            return {
                {"content", json::array({{{"type", "text"}, {"text", "Missing or invalid 'index' argument"}}})},
                {"isError", true}
            };
        }

        int index = args["index"].get<int>();
        if (index < 0) {
            return {
                {"content", json::array({{{"type", "text"}, {"text", "Index must be non-negative"}}})},
                {"isError", true}
            };
        }

        ModelInfo info;
        if (!accessor_->getModelInfo(index, info)) {
            return {
                {"content", json::array({{{"type", "text"}, {"text", "Model not found at index " + std::to_string(index)}}})},
                {"isError", true}
            };
        }

        auto bones = accessor_->getBones(index);
        json bonesArr = json::array();
        for (int i = 0; i < static_cast<int>(bones.size()); ++i) {
            bonesArr.push_back({
                {"index", i},
                {"name_jp", bones[i].name_jp},
                {"name_en", bones[i].name_en}
            });
        }

        auto [morphsOk, morphs] = accessor_->getMorphs(index);
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

        json result = {
            {"index", index},
            {"name_jp", info.name_jp},
            {"name_en", info.name_en},
            {"comment_jp", info.comment_jp},
            {"file_path", info.file_path},
            {"bone_count", info.bone_count},
            {"morph_count", info.morph_count},
            {"ik_count", info.ik_count},
            {"is_visible", info.is_visible},
            {"bones", bonesArr},
            {"morphs", morphsArr},
            {"morph_source", morphsOk ? "pmx" : "unavailable"}
        };

        return {
            {"content", json::array({{{"type", "text"}, {"text", result.dump()}}})},
            {"isError", false}
        };
    }

private:
    IModelAccessor* accessor_;
};
