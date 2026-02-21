#pragma once

#include "tool.h"
#include "tools/camera/camera_accessor.h"
#include "common/frame_set.h"
#include <string>
#include <vector>

// 補間チャンネルインデックス: hokan*[0]=x, [1]=y, [2]=z, [3]=rotation, [4]=distance, [5]=fov
static const char* INTERP_CHANNEL_NAMES[] = {"x", "y", "z", "rotation", "distance", "fov"};
static constexpr int INTERP_CHANNEL_COUNT = 6;

static nlohmann::json keyframeToJson(const mmp::CameraKeyFrameData& kf, int frame) {
    using json = nlohmann::json;

    json interp = json::object();
    for (int i = 0; i < INTERP_CHANNEL_COUNT; ++i) {
        interp[INTERP_CHANNEL_NAMES[i]] = {
            {"x1", static_cast<int>(static_cast<unsigned char>(kf.hokan1_x[i]))},
            {"y1", static_cast<int>(static_cast<unsigned char>(kf.hokan1_y[i]))},
            {"x2", static_cast<int>(static_cast<unsigned char>(kf.hokan2_x[i]))},
            {"y2", static_cast<int>(static_cast<unsigned char>(kf.hokan2_y[i]))}
        };
    }

    return {
        {"frame", frame},
        {"position", {{"x", kf.xyz.x}, {"y", kf.xyz.y}, {"z", kf.xyz.z}}},
        {"rotation", {{"x", kf.rxyz.x}, {"y", kf.rxyz.y}, {"z", kf.rxyz.z}}},
        {"distance", kf.length},
        {"fov", kf.view_angle},
        {"perspective", kf.is_perspective == 0},
        {"selected", kf.is_selected != 0},
        {"look_at_model", kf.looking_model_index},
        {"look_at_bone", kf.looking_bone_index},
        {"interpolation", interp}
    };
}

// JSON → CameraKeyFrameData 適用ヘルパー（Create/Update共用）
static void applyVec3(mmp::Float3& target, const nlohmann::json& j) {
    if (j.contains("x") && j["x"].is_number()) target.x = j["x"].get<float>();
    if (j.contains("y") && j["y"].is_number()) target.y = j["y"].get<float>();
    if (j.contains("z") && j["z"].is_number()) target.z = j["z"].get<float>();
}

static bool applyInterpChannel(char& x1, char& y1, char& x2, char& y2,
                                const nlohmann::json& j) {
    auto clamp = [](const nlohmann::json& obj, const char* key, char& out) -> bool {
        if (obj.contains(key) && obj[key].is_number_integer()) {
            int v = obj[key].get<int>();
            if (v < 0 || v > 127) return false;
            out = static_cast<char>(v);
        }
        return true;
    };
    return clamp(j, "x1", x1) && clamp(j, "y1", y1)
        && clamp(j, "x2", x2) && clamp(j, "y2", y2);
}

// 事前バリデーション: applyJson を呼ぶ前に入力値を検証する
// 戻り値: 成功=空文字列、失敗=エラーメッセージ
static std::string validateJson(const nlohmann::json& j) {
    if (j.contains("interpolation") && j["interpolation"].is_object()) {
        auto& interp = j["interpolation"];
        for (int i = 0; i < INTERP_CHANNEL_COUNT; ++i) {
            if (interp.contains(INTERP_CHANNEL_NAMES[i]) && interp[INTERP_CHANNEL_NAMES[i]].is_object()) {
                auto& ch = interp[INTERP_CHANNEL_NAMES[i]];
                for (const char* key : {"x1", "y1", "x2", "y2"}) {
                    if (ch.contains(key) && ch[key].is_number_integer()) {
                        int v = ch[key].get<int>();
                        if (v < 0 || v > 127) {
                            return std::string("Interpolation values must be 0-127 (channel: ")
                                + INTERP_CHANNEL_NAMES[i] + ", " + key + "=" + std::to_string(v) + ")";
                        }
                    }
                }
            }
        }
    }
    return "";
}

// JSON → CameraKeyFrameData 適用（validateJson で検証済みの前提）
static void applyJson(mmp::CameraKeyFrameData& kf, const nlohmann::json& j) {
    if (j.contains("position") && j["position"].is_object())
        applyVec3(kf.xyz, j["position"]);
    if (j.contains("rotation") && j["rotation"].is_object())
        applyVec3(kf.rxyz, j["rotation"]);
    if (j.contains("distance") && j["distance"].is_number())
        kf.length = j["distance"].get<float>();
    if (j.contains("fov") && j["fov"].is_number_integer())
        kf.view_angle = j["fov"].get<int>();
    if (j.contains("perspective") && j["perspective"].is_boolean())
        kf.is_perspective = j["perspective"].get<bool>() ? 0 : 1;
    if (j.contains("selected") && j["selected"].is_boolean())
        kf.is_selected = j["selected"].get<bool>() ? 1 : 0;
    if (j.contains("look_at_model") && j["look_at_model"].is_number_integer())
        kf.looking_model_index = j["look_at_model"].get<int>();
    if (j.contains("look_at_bone") && j["look_at_bone"].is_number_integer())
        kf.looking_bone_index = j["look_at_bone"].get<int>();

    if (j.contains("interpolation") && j["interpolation"].is_object()) {
        auto& interp = j["interpolation"];
        for (int i = 0; i < INTERP_CHANNEL_COUNT; ++i) {
            if (interp.contains(INTERP_CHANNEL_NAMES[i]) && interp[INTERP_CHANNEL_NAMES[i]].is_object()) {
                applyInterpChannel(
                    kf.hokan1_x[i], kf.hokan1_y[i],
                    kf.hokan2_x[i], kf.hokan2_y[i],
                    interp[INTERP_CHANNEL_NAMES[i]]);
            }
        }
    }
}

// --- スキーマヘルパー ---

static nlohmann::json cameraKeyframeSchema(bool allRequired) {
    using json = nlohmann::json;
    json pointSchema = {
        {"type", "object"},
        {"properties", {
            {"x1", {{"type", "integer"}, {"minimum", 0}, {"maximum", 127}}},
            {"y1", {{"type", "integer"}, {"minimum", 0}, {"maximum", 127}}},
            {"x2", {{"type", "integer"}, {"minimum", 0}, {"maximum", 127}}},
            {"y2", {{"type", "integer"}, {"minimum", 0}, {"maximum", 127}}}
        }}
    };
    json interpSchema = {
        {"type", "object"},
        {"properties", {
            {"x", pointSchema}, {"y", pointSchema}, {"z", pointSchema},
            {"rotation", pointSchema}, {"distance", pointSchema}, {"fov", pointSchema}
        }}
    };
    json vec3Schema = {
        {"type", "object"},
        {"properties", {
            {"x", {{"type", "number"}}},
            {"y", {{"type", "number"}}},
            {"z", {{"type", "number"}}}
        }}
    };
    json rotationSchema = {
        {"type", "object"},
        {"description", "Rotation in radians (e.g. 0.1745 rad = 10 degrees)"},
        {"properties", {
            {"x", {{"type", "number"}}},
            {"y", {{"type", "number"}}},
            {"z", {{"type", "number"}}}
        }}
    };
    json schema = {
        {"type", "object"},
        {"properties", {
            {"frame", {{"type", "integer"}, {"minimum", 0}}},
            {"position", vec3Schema},
            {"rotation", rotationSchema},
            {"distance", {{"type", "number"}}},
            {"fov", {{"type", "integer"}}},
            {"perspective", {{"type", "boolean"}}},
            {"selected", {{"type", "boolean"}}},
            {"look_at_model", {{"type", "integer"}}},
            {"look_at_bone", {{"type", "integer"}}},
            {"interpolation", interpSchema}
        }}
    };
    if (allRequired) {
        schema["required"] = json::array({"frame", "position", "rotation", "distance", "fov", "perspective", "interpolation"});
    } else {
        schema["required"] = json::array({"frame"});
    }
    return schema;
}

// =====================================================================
// Read
// =====================================================================
class GetCameraKeyframesTool : public ITool {
public:
    explicit GetCameraKeyframesTool(ICameraKeyframeAccessor* accessor)
        : accessor_(accessor) {}

    std::string name() const override { return "get_camera_keyframes"; }
    std::string description() const override { return "Get camera keyframe data for specified frames"; }

    nlohmann::json inputSchema() const override {
        using json = nlohmann::json;
        return {
            {"type", "object"},
            {"properties", {
                {"frames", {
                    {"type", "string"},
                    {"description", "Frame range (e.g. \"0-10\", \"1-3,5,8-10\"). If omitted, returns all keyframes."}
                }}
            }}
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

        FrameSet frameSet;
        bool filterByRange = false;
        if (args.contains("frames") && args["frames"].is_string()) {
            try {
                frameSet = FrameSet::parse(args["frames"].get<std::string>());
                filterByRange = true;
            } catch (const std::invalid_argument& e) {
                return {
                    {"content", json::array({{{"type", "text"}, {"text", std::string("Invalid frame range: ") + e.what()}}})},
                    {"isError", true}
                };
            }
        }

        json keyframes = json::array();
        accessor_->forEachCameraKeyframe([&](const mmp::CameraKeyFrameData& kf) {
            if (!filterByRange || frameSet.contains(kf.frame_no)) {
                keyframes.push_back(keyframeToJson(kf, kf.frame_no));
            }
        });

        json result = {{"keyframes", keyframes}};
        return {
            {"content", json::array({{{"type", "text"}, {"text", result.dump()}}})},
            {"isError", false}
        };
    }

private:
    ICameraKeyframeAccessor* accessor_;
};

// =====================================================================
// Create
// =====================================================================
class CreateCameraKeyframesTool : public ITool {
public:
    explicit CreateCameraKeyframesTool(ICameraKeyframeAccessor* accessor)
        : accessor_(accessor) {}

    std::string name() const override { return "create_camera_keyframes"; }
    std::string description() const override { return "Create new camera keyframes. Fails if keyframe already exists at the specified frame."; }

    nlohmann::json inputSchema() const override {
        using json = nlohmann::json;
        return {
            {"type", "object"},
            {"properties", {
                {"keyframes", {
                    {"type", "array"},
                    {"items", cameraKeyframeSchema(true)},
                    {"description", "Array of keyframe objects. All fields except 'selected', 'look_at_model', 'look_at_bone' are required."}
                }}
            }},
            {"required", json::array({"keyframes"})}
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

        if (!args.contains("keyframes") || !args["keyframes"].is_array()) {
            return {
                {"content", json::array({{{"type", "text"}, {"text", "Missing or invalid 'keyframes' array"}}})},
                {"isError", true}
            };
        }

        auto& keyframes = args["keyframes"];
        if (keyframes.empty()) {
            return {
                {"content", json::array({{{"type", "text"}, {"text", "Empty keyframes array"}}})},
                {"isError", true}
            };
        }

        static const char* REQUIRED_FIELDS[] = {
            "frame", "position", "rotation", "distance", "fov", "perspective", "interpolation"
        };

        int created = 0;
        std::string errors;

        for (auto& kfJson : keyframes) {
            std::string missing;
            for (auto& field : REQUIRED_FIELDS) {
                if (!kfJson.contains(field)) {
                    if (!missing.empty()) missing += ", ";
                    missing += field;
                }
            }
            if (!missing.empty()) {
                errors += "Missing required fields: " + missing + "; ";
                continue;
            }

            if (!kfJson["frame"].is_number_integer()) {
                errors += "Keyframe 'frame' must be integer; ";
                continue;
            }
            int frame = kfJson["frame"].get<int>();
            if (frame < 0 || frame >= mmd_mcp::MAX_CAMERA_FRAMES) {
                errors += "Frame " + std::to_string(frame) + " out of range; ";
                continue;
            }

            std::string valErr = validateJson(kfJson);
            if (!valErr.empty()) {
                errors += "Frame " + std::to_string(frame) + ": " + valErr + "; ";
                continue;
            }

            auto result = accessor_->createCameraKeyframe(frame, [&](mmp::CameraKeyFrameData& kf) {
                applyJson(kf, kfJson);
            });
            if (result == KeyframeResult::Success) {
                ++created;
            } else if (result == KeyframeResult::AlreadyExists) {
                errors += "Keyframe already exists at frame " + std::to_string(frame) + "; ";
            } else {
                errors += "Failed to create keyframe at frame " + std::to_string(frame) + "; ";
            }
        }

        if (created == 0 && !errors.empty()) {
            return {
                {"content", json::array({{{"type", "text"}, {"text", errors}}})},
                {"isError", true}
            };
        }

        std::string msg = "Created " + std::to_string(created) + " keyframe" + (created != 1 ? "s" : "");
        if (!errors.empty()) msg += " (warnings: " + errors + ")";
        return {
            {"content", json::array({{{"type", "text"}, {"text", msg}}})},
            {"isError", false}
        };
    }

private:
    ICameraKeyframeAccessor* accessor_;
};

// =====================================================================
// Update
// =====================================================================
class UpdateCameraKeyframesTool : public ITool {
public:
    explicit UpdateCameraKeyframesTool(ICameraKeyframeAccessor* accessor)
        : accessor_(accessor) {}

    std::string name() const override { return "update_camera_keyframes"; }
    std::string description() const override { return "Update existing camera keyframes. Fails if keyframe does not exist at the specified frame."; }

    nlohmann::json inputSchema() const override {
        using json = nlohmann::json;
        return {
            {"type", "object"},
            {"properties", {
                {"keyframes", {
                    {"type", "array"},
                    {"items", cameraKeyframeSchema(false)},
                    {"description", "Array of keyframe objects. Only 'frame' is required; other fields are optional (partial update)."}
                }}
            }},
            {"required", json::array({"keyframes"})}
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

        if (!args.contains("keyframes") || !args["keyframes"].is_array()) {
            return {
                {"content", json::array({{{"type", "text"}, {"text", "Missing or invalid 'keyframes' array"}}})},
                {"isError", true}
            };
        }

        auto& keyframes = args["keyframes"];
        if (keyframes.empty()) {
            return {
                {"content", json::array({{{"type", "text"}, {"text", "Empty keyframes array"}}})},
                {"isError", true}
            };
        }

        int updated = 0;
        std::string errors;

        for (auto& kfJson : keyframes) {
            if (!kfJson.contains("frame") || !kfJson["frame"].is_number_integer()) {
                errors += "Keyframe missing 'frame' field; ";
                continue;
            }
            int frame = kfJson["frame"].get<int>();
            if (frame < 0 || frame >= mmd_mcp::MAX_CAMERA_FRAMES) {
                errors += "Frame " + std::to_string(frame) + " out of range; ";
                continue;
            }

            std::string valErr = validateJson(kfJson);
            if (!valErr.empty()) {
                errors += "Frame " + std::to_string(frame) + ": " + valErr + "; ";
                continue;
            }

            auto result = accessor_->updateCameraKeyframe(frame, [&](mmp::CameraKeyFrameData& kf) {
                applyJson(kf, kfJson);
            });
            if (result == KeyframeResult::Success) {
                ++updated;
            } else if (result == KeyframeResult::NotFound) {
                errors += "No keyframe exists at frame " + std::to_string(frame) + "; ";
            } else {
                errors += "Failed to update keyframe at frame " + std::to_string(frame) + "; ";
            }
        }

        if (updated == 0 && !errors.empty()) {
            return {
                {"content", json::array({{{"type", "text"}, {"text", errors}}})},
                {"isError", true}
            };
        }

        std::string msg = "Updated " + std::to_string(updated) + " keyframe" + (updated != 1 ? "s" : "");
        if (!errors.empty()) msg += " (warnings: " + errors + ")";
        return {
            {"content", json::array({{{"type", "text"}, {"text", msg}}})},
            {"isError", false}
        };
    }

private:
    ICameraKeyframeAccessor* accessor_;
};

// =====================================================================
// Delete
// =====================================================================
class DeleteCameraKeyframesTool : public ITool {
public:
    explicit DeleteCameraKeyframesTool(ICameraKeyframeAccessor* accessor)
        : accessor_(accessor) {}

    std::string name() const override { return "delete_camera_keyframes"; }
    std::string description() const override { return "Delete camera keyframes at specified frames. Frame 0 cannot be deleted (it is the base keyframe)."; }

    nlohmann::json inputSchema() const override {
        using json = nlohmann::json;
        return {
            {"type", "object"},
            {"properties", {
                {"frames", {
                    {"type", "string"},
                    {"description", "Frame range to delete (e.g. \"5,10-20,30\")"}
                }}
            }},
            {"required", json::array({"frames"})}
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

        if (!args.contains("frames") || !args["frames"].is_string()) {
            return {
                {"content", json::array({{{"type", "text"}, {"text", "Missing or invalid 'frames' parameter"}}})},
                {"isError", true}
            };
        }

        FrameSet frameSet;
        try {
            frameSet = FrameSet::parse(args["frames"].get<std::string>());
        } catch (const std::invalid_argument& e) {
            return {
                {"content", json::array({{{"type", "text"}, {"text", std::string("Invalid frame range: ") + e.what()}}})},
                {"isError", true}
            };
        }

        if (frameSet.empty()) {
            return {
                {"content", json::array({{{"type", "text"}, {"text", "No frames in specified range"}}})},
                {"isError", true}
            };
        }

        // マッチするキーフレームのフレーム番号を収集（フレーム0は除外）
        bool skippedFrame0 = false;
        std::vector<int> targets;
        accessor_->forEachCameraKeyframe([&](const mmp::CameraKeyFrameData& kf) {
            if (frameSet.contains(kf.frame_no)) {
                if (kf.frame_no == 0) {
                    skippedFrame0 = true;
                } else {
                    targets.push_back(kf.frame_no);
                }
            }
        });

        if (targets.empty()) {
            std::string msg = skippedFrame0
                ? "Frame 0 cannot be deleted (it is the base keyframe)"
                : "No keyframes found in specified range";
            return {
                {"content", json::array({{{"type", "text"}, {"text", msg}}})},
                {"isError", true}
            };
        }

        // インデックスシフトを避けるため逆順に削除
        std::sort(targets.rbegin(), targets.rend());

        int deleted = 0;
        std::string warnings;

        if (skippedFrame0) {
            warnings += "Frame 0 cannot be deleted (it is the base keyframe); ";
        }

        for (int frame : targets) {
            auto result = accessor_->deleteCameraKeyframe(frame);
            if (result == KeyframeResult::Success) {
                ++deleted;
            } else {
                warnings += "Failed to delete keyframe at frame " + std::to_string(frame) + "; ";
            }
        }

        if (deleted == 0 && !warnings.empty()) {
            return {
                {"content", json::array({{{"type", "text"}, {"text", warnings}}})},
                {"isError", true}
            };
        }

        std::string msg = "Deleted " + std::to_string(deleted) + " keyframe" + (deleted != 1 ? "s" : "");
        if (!warnings.empty()) msg += " (warnings: " + warnings + ")";
        return {
            {"content", json::array({{{"type", "text"}, {"text", msg}}})},
            {"isError", false}
        };
    }

private:
    ICameraKeyframeAccessor* accessor_;
};
