#pragma once

#include "tool.h"
#include "tools/morph/morph_accessor.h"
#include "tools/model/model_accessor.h"
#include "common/frame_set.h"

class GetMorphKeyframesTool : public ITool {
public:
    GetMorphKeyframesTool(IMorphKeyframeAccessor* accessor, IModelAccessor* modelAccessor = nullptr)
        : accessor_(accessor), modelAccessor_(modelAccessor) {}

    std::string name() const override { return "get_morph_keyframes"; }
    std::string description() const override {
        return "Get morph keyframe data for a specific morph of a model. "
               "Specify morph by morph_index (0-based) or morph_name (Japanese name, exact match). "
               "Exactly one of morph_index or morph_name is required.";
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
                    {"description", "Morph index (0 to morph_count-1). Specify either morph_index or morph_name."}
                }},
                {"morph_name", {
                    {"type", "string"},
                    {"description", "Morph name in Japanese (exact match on name_jp). Specify either morph_index or morph_name."}
                }},
                {"frames", {
                    {"type", "string"},
                    {"description", "Frame range (e.g. \"0-10\", \"1-3,5,8-10\"). "
                                    "If omitted, returns all keyframes."}
                }}
            }},
            {"required", json::array({"model_index"})},
            {"additionalProperties", false}
        };
    }

    nlohmann::json execute(const nlohmann::json& args) override {
        using json = nlohmann::json;

        if (!accessor_)
            return errorResult("MMD data not available");

        if (!args.contains("model_index") || !args["model_index"].is_number_integer())
            return errorResult("Missing or invalid 'model_index'");

        int modelIndex = args["model_index"].get<int>();
        if (modelIndex < 0)
            return errorResult("model_index must be >= 0");

        bool hasIndex = args.contains("morph_index") && args["morph_index"].is_number_integer();
        bool hasName = args.contains("morph_name") && args["morph_name"].is_string();

        if (hasIndex && hasName)
            return errorResult("Specify either morph_index or morph_name, not both");
        if (!hasIndex && !hasName)
            return errorResult("Either morph_index or morph_name is required");

        int morphIndex = -1;

        if (hasIndex) {
            morphIndex = args["morph_index"].get<int>();
        } else {
            // morph_name による名前解決
            if (!modelAccessor_)
                return errorResult("morph_name resolution not available");

            auto morphName = args["morph_name"].get<std::string>();
            auto [pmxStatus, morphs] = modelAccessor_->getMorphs(modelIndex);
            if (pmxStatus == PmxStatus::file_modified_after_launch)
                return errorResult(
                    "The PMX file for this model was modified after MMD was launched. "
                    "Morph data may be inconsistent. "
                    "Please ask the user to restart MMD and try again.");
            if (pmxStatus != PmxStatus::ok)
                return errorResult("Failed to resolve morph name (PMX parse failed for model " +
                                   std::to_string(modelIndex) + ")");

            for (int i = 0; i < static_cast<int>(morphs.size()); ++i) {
                if (morphs[i].name_jp == morphName) {
                    morphIndex = i;
                    break;
                }
            }
            if (morphIndex < 0)
                return errorResult("Morph '" + morphName + "' not found in model " +
                                   std::to_string(modelIndex));
        }

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
    IModelAccessor* modelAccessor_;

    static nlohmann::json errorResult(const std::string& msg) {
        using json = nlohmann::json;
        return {
            {"content", json::array({{{"type", "text"}, {"text", msg}}})},
            {"isError", true}
        };
    }
};

class GetAllMorphKeyframesTool : public ITool {
public:
    GetAllMorphKeyframesTool(IMorphKeyframeAccessor* accessor, IModelAccessor* modelAccessor)
        : accessor_(accessor), modelAccessor_(modelAccessor) {}

    std::string name() const override { return "get_all_morph_keyframes"; }
    std::string description() const override {
        return "Get keyframes for ALL morphs of a model at specified frames. "
               "Returns only morphs that have keyframes (non-zero by default). "
               "Much more efficient than calling get_morph_keyframes for each morph individually.";
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
                {"frames", {
                    {"type", "string"},
                    {"description", "Frame range (e.g. \"100\", \"0-10\", \"1-3,5,8-10\"). "
                                    "If omitted, returns all keyframes for all morphs."}
                }},
                {"include_zero", {
                    {"type", "boolean"},
                    {"description", "If true, include morphs where all matched keyframes have value 0. Default: false."}
                }}
            }},
            {"required", json::array({"model_index"})},
            {"additionalProperties", false}
        };
    }

    nlohmann::json execute(const nlohmann::json& args) override {
        using json = nlohmann::json;

        if (!accessor_)
            return errorResult("MMD data not available");

        if (!args.contains("model_index") || !args["model_index"].is_number_integer())
            return errorResult("Missing or invalid 'model_index'");

        int modelIndex = args["model_index"].get<int>();
        if (modelIndex < 0)
            return errorResult("model_index must be >= 0");

        int morphCount = accessor_->getMorphCount(modelIndex);
        if (morphCount < 0)
            return errorResult("Model not found at index " + std::to_string(modelIndex));

        bool includeZero = false;
        if (args.contains("include_zero") && args["include_zero"].is_boolean())
            includeZero = args["include_zero"].get<bool>();

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

        // PMXからモーフ名を取得（キャッシュ済み）
        std::vector<MorphBasicInfo> morphNames;
        bool hasNames = false;
        if (modelAccessor_) {
            auto [pmxStatus, morphs] = modelAccessor_->getMorphs(modelIndex);
            if (pmxStatus == PmxStatus::file_modified_after_launch)
                return errorResult(
                    "The PMX file for this model was modified after MMD was launched. "
                    "Morph data may be inconsistent. "
                    "Please ask the user to restart MMD and try again.");
            if (pmxStatus == PmxStatus::ok) {
                morphNames = std::move(morphs);
                hasNames = true;
            }
        }

        json morphsArr = json::array();
        for (int i = 0; i < morphCount; ++i) {
            json keyframes = json::array();
            bool hasNonZero = false;

            accessor_->forEachMorphKeyframe(modelIndex, i,
                [&](const MorphKeyFrame& kf) {
                    if (!filterByRange || frameSet.contains(kf.frame_number)) {
                        keyframes.push_back({
                            {"frame", kf.frame_number},
                            {"value", kf.value}
                        });
                        if (kf.value != 0.0f) hasNonZero = true;
                    }
                });

            if (keyframes.empty() && !includeZero) continue;
            if (!hasNonZero && !includeZero) continue;

            json entry = {{"morph_index", i}, {"keyframes", keyframes}};
            if (hasNames && i < static_cast<int>(morphNames.size())) {
                entry["name_jp"] = morphNames[i].name_jp;
            }
            morphsArr.push_back(entry);
        }

        json result = {{"morphs", morphsArr}};
        return {
            {"content", json::array({{{"type", "text"}, {"text", result.dump()}}})},
            {"isError", false}
        };
    }

private:
    IMorphKeyframeAccessor* accessor_;
    IModelAccessor* modelAccessor_;

    static nlohmann::json errorResult(const std::string& msg) {
        using json = nlohmann::json;
        return {
            {"content", json::array({{{"type", "text"}, {"text", msg}}})},
            {"isError", true}
        };
    }
};
