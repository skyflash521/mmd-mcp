#pragma once

#include "tools/morph/morph_accessor.h"
#include <map>
#include <functional>

#ifdef MMD_MCP_TEST
#include "common/keyframe_traits.h"

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
#endif

class MockMorphKeyframeAccessor : public IMorphKeyframeAccessor {
public:
    void seed(int model_index, int morph_index, int frame, float value) {
        MorphKeyFrame kf{};
        kf.frame_number = frame;
        kf.pre_index = 0;
        kf.next_index = 0;
        kf.value = value;
        kf.is_selected = 0;
        store_[model_index][morph_index][frame] = kf;
    }

    void setMorphCount(int model_index, int count) {
        morph_counts_[model_index] = count;
    }

    bool forEachMorphKeyframe(int model_index, int morph_index,
        const std::function<void(const MorphKeyFrame&)>& visitor) const override {
        auto cit = morph_counts_.find(model_index);
        if (cit == morph_counts_.end()) return false;
        if (morph_index < 0 || morph_index >= cit->second) return false;

        auto mit = store_.find(model_index);
        if (mit == store_.end()) return true;
        auto sit = mit->second.find(morph_index);
        if (sit == mit->second.end()) return true;

        for (auto& [frame, kf] : sit->second) {
            visitor(kf);
        }
        return true;
    }

    int getMorphCount(int model_index) const override {
        auto it = morph_counts_.find(model_index);
        return it != morph_counts_.end() ? it->second : -1;
    }

private:
    std::map<int, std::map<int, std::map<int, MorphKeyFrame>>> store_;
    std::map<int, int> morph_counts_;
};
