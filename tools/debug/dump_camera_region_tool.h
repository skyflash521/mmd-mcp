#pragma once

#include "tool.h"
#include "mmd_plugin.h"
#include "common/keyframe_traits.h"
#include "common/keyframe_linked_list.h"
#include <cstring>

// camera_key_frame配列のスロット割り当てパターンと周辺メモリをダンプする調査ツール。
class DumpCameraRegionTool : public ITool {
public:
    std::string name() const override { return "dump_camera_region"; }
    std::string description() const override {
        return "Dump camera_key_frame slot allocation and surrounding memory for investigation";
    }

    nlohmann::json inputSchema() const override {
        using json = nlohmann::json;
        return {
            {"type", "object"},
            {"properties", {
                {"slot_count", {{"type", "integer"}, {"description", "Number of camera_key_frame slots to dump (default 10)"}}},
                {"scan_value", {{"type", "integer"}, {"description", "Optional: scan entire MMDMainData (excl. camera array) for this int value"}}}
            }}
        };
    }

    nlohmann::json execute(const nlohmann::json& args) override {
        using json = nlohmann::json;
        auto* data = mmp::getMMDMainData();
        if (!data) {
            return {
                {"content", json::array({{{"type", "text"}, {"text", "MMD data not available"}}})},
                {"isError", true}
            };
        }

        int slotCount = args.value("slot_count", 10);
        if (slotCount < 1) slotCount = 1;
        if (slotCount > 100) slotCount = 100;

        // camera_key_frame スロットのダンプ
        json slots = json::array();
        for (int i = 0; i < slotCount && i < 10000; ++i) {
            auto& kf = data->camera_key_frame[i];
            json slot = {
                {"index", i},
                {"frame_no", kf.frame_no},
                {"pre_index", kf.pre_index},
                {"next_index", kf.next_index},
                {"is_selected", kf.is_selected}
            };
            // 空スロットかどうか
            if (kf.frame_no == 0 && kf.pre_index == 0 && kf.next_index == 0 && i > 0) {
                slot["status"] = "empty";
            } else {
                slot["status"] = "used";
                slot["length"] = kf.length;
                slot["view_angle"] = kf.view_angle;
                slot["is_perspective"] = kf.is_perspective;
            }
            slots.push_back(slot);
        }

        // リンクリスト走査によるキーフレーム数
        using KF = mmp::CameraKeyFrameData;
        using Traits = mmd_mcp::KeyframeTraits<KF>;
        int linkedListCount = mmd_mcp::countKeyframes<KF, Traits>(
            &data->camera_key_frame[0], mmd_mcp::MAX_CAMERA_FRAMES);

        // camera_key_frame 直前の88バイト (__unknown60[22]) をダンプ
        json pre_region = json::array();
        for (int i = 0; i < 22; ++i) {
            pre_region.push_back(data->__unknown60[i]);
        }

        json result = {
            {"linked_list_count", linkedListCount},
            {"slots", slots},
            {"__unknown60", pre_region}
        };

        // オプション: 全体スキャン
        if (args.contains("scan_value")) {
            int searchVal = args["scan_value"].get<int>();
            size_t camStart = offsetof(mmp::MMDMainData, camera_key_frame);
            size_t camEnd = camStart + sizeof(mmp::CameraKeyFrameData) * 10000;
            auto* base = reinterpret_cast<const unsigned char*>(data);
            size_t totalSize = sizeof(mmp::MMDMainData);

            json matches = json::array();
            for (size_t off = 0; off + sizeof(int) <= totalSize; off += sizeof(int)) {
                if (off >= camStart && off < camEnd) continue;
                int val;
                std::memcpy(&val, base + off, sizeof(int));
                if (val == searchVal) {
                    matches.push_back(off);
                }
            }
            result["scan_value"] = searchVal;
            result["scan_matches"] = matches;
        }

        return {
            {"content", json::array({{{"type", "text"}, {"text", result.dump(2)}}})},
            {"isError", false}
        };
    }
};
