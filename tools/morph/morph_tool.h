#pragma once

#include "tool.h"
#include "tools/morph/morph_accessor.h"
#include "common/frame_set.h"

class GetMorphKeyframesTool : public ITool {
public:
    explicit GetMorphKeyframesTool(IMorphKeyframeAccessor* accessor)
        : accessor_(accessor) {}

    std::string name() const override { return "get_morph_keyframes"; }
    std::string description() const override {
        return "Get morph keyframe data for a specific morph of a model. "
               "Use list_models to find model_index and get_model_info for morph_count. "
               "Morph indices are 0-based (0 to morph_count-1).";
    }

    nlohmann::json inputSchema() const override {
        using json = nlohmann::json;
        return {
            {"type", "object"},
            {"properties", {
                {"model_index", {
                    {"type", "integer"},
                    {"minimum", 0},
                    {"description", "Model index (from list_models)"}
                }},
                {"morph_index", {
                    {"type", "integer"},
                    {"minimum", 0},
                    {"description", "Morph index (0 to morph_count-1, from get_model_info)"}
                }},
                {"frames", {
                    {"type", "string"},
                    {"description", "Frame range (e.g. \"0-10\", \"1-3,5,8-10\"). "
                                    "If omitted, returns all keyframes."}
                }}
            }},
            {"required", json::array({"model_index", "morph_index"})},
            {"additionalProperties", false}
        };
    }

    nlohmann::json execute(const nlohmann::json& args) override {
        using json = nlohmann::json;

        if (!accessor_)
            return errorResult("MMD data not available");

        if (!args.contains("model_index") || !args["model_index"].is_number_integer())
            return errorResult("Missing or invalid 'model_index'");
        if (!args.contains("morph_index") || !args["morph_index"].is_number_integer())
            return errorResult("Missing or invalid 'morph_index'");

        int modelIndex = args["model_index"].get<int>();
        int morphIndex = args["morph_index"].get<int>();

        if (modelIndex < 0)
            return errorResult("model_index must be >= 0");

        int morphCount = accessor_->getMorphCount(modelIndex);
        if (morphCount < 0)
            return errorResult("Model not found at index " + std::to_string(modelIndex));
        if (morphIndex < 0 || morphIndex >= morphCount)
            return errorResult("Morph index " + std::to_string(morphIndex) +
                             " out of range (model has " + std::to_string(morphCount) + " morphs)");

        FrameSet frameSet;
        bool filterByRange = false;
        if (args.contains("frames") && args["frames"].is_string()) {
            try {
                frameSet = FrameSet::parse(args["frames"].get<std::string>());
                filterByRange = true;
            } catch (const std::invalid_argument& e) {
                return errorResult(std::string("Invalid frame range: ") + e.what());
            }
        }

        json keyframes = json::array();
        bool ok = accessor_->forEachMorphKeyframe(modelIndex, morphIndex,
            [&](const MorphKeyFrame& kf) {
                if (!filterByRange || frameSet.contains(kf.frame_number)) {
                    keyframes.push_back({
                        {"frame", kf.frame_number},
                        {"value", kf.value},
                        {"selected", kf.is_selected != 0}
                    });
                }
            });

        if (!ok)
            return errorResult("Failed to access morph keyframes");

        json result = {{"keyframes", keyframes}};
        return {
            {"content", json::array({{{"type", "text"}, {"text", result.dump()}}})},
            {"isError", false}
        };
    }

private:
    IMorphKeyframeAccessor* accessor_;

    static nlohmann::json errorResult(const std::string& msg) {
        using json = nlohmann::json;
        return {
            {"content", json::array({{{"type", "text"}, {"text", msg}}})},
            {"isError", true}
        };
    }
};
