#pragma once

#include <functional>

namespace mmd_mcp {
    static constexpr int MAX_MORPH_FRAMES = 10000;
}

// mmp::MMDModelData::MorphKeyFrame と同一メモリレイアウトのスタンドアロン構造体。
// ネスト型は前方宣言できないため、インターフェース用に独立定義する。
struct MorphKeyFrame {
    int frame_number;
    int pre_index;
    int next_index;
    float value;
    char is_selected;
};

class IMorphKeyframeAccessor {
public:
    virtual ~IMorphKeyframeAccessor() = default;

    // 指定モデルの指定モーフのキーフレームを走査。false = モデルまたはモーフが見つからない
    virtual bool forEachMorphKeyframe(
        int model_index, int morph_index,
        const std::function<void(const MorphKeyFrame&)>& visitor) const = 0;

    // モーフ数を返す。-1 = モデルが見つからない
    virtual int getMorphCount(int model_index) const = 0;
};

#ifndef MMD_MCP_TEST
#include "mmd_plugin.h"
#include "common/keyframe_traits.h"
#include "common/keyframe_linked_list.h"

static_assert(sizeof(MorphKeyFrame) == sizeof(mmp::MMDModelData::MorphKeyFrame),
    "MorphKeyFrame layout mismatch");

namespace mmd_mcp {

template<>
struct KeyframeTraits<MorphKeyFrame> {
    static int frameNo(const MorphKeyFrame& kf) { return kf.frame_number; }
    static void setFrameNo(MorphKeyFrame& kf, int f) { kf.frame_number = f; }
    static int preIndex(const MorphKeyFrame& kf) { return kf.pre_index; }
    static void setPreIndex(MorphKeyFrame& kf, int i) { kf.pre_index = i; }
    static int nextIndex(const MorphKeyFrame& kf) { return kf.next_index; }
    static void setNextIndex(MorphKeyFrame& kf, int i) { kf.next_index = i; }
};

} // namespace mmd_mcp

class MmdMorphKeyframeAccessor : public IMorphKeyframeAccessor {
    using Traits = mmd_mcp::KeyframeTraits<MorphKeyFrame>;
    static constexpr int MAX_MODELS = 255;

public:
    bool forEachMorphKeyframe(int model_index, int morph_index,
        const std::function<void(const MorphKeyFrame&)>& visitor) const override {
        auto* data = mmp::getMMDMainData();
        if (!data) return false;
        if (model_index < 0 || model_index >= MAX_MODELS) return false;
        auto* model = data->model_data[model_index];
        if (!model) return false;
        if (morph_index < 0 || morph_index >= model->morph_count) return false;
        if (!model->morph_keyframe) return false;

        auto* arr = reinterpret_cast<MorphKeyFrame*>(
            &model->morph_keyframe[morph_index * mmd_mcp::MAX_MORPH_FRAMES]);
        mmd_mcp::forEachKeyframe<MorphKeyFrame, Traits>(
            arr, mmd_mcp::MAX_MORPH_FRAMES, visitor);
        return true;
    }

    int getMorphCount(int model_index) const override {
        auto* data = mmp::getMMDMainData();
        if (!data) return -1;
        if (model_index < 0 || model_index >= MAX_MODELS) return -1;
        auto* model = data->model_data[model_index];
        if (!model) return -1;
        return model->morph_count;
    }
};
#endif
