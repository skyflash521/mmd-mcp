#pragma once

#include "tools/camera/camera_accessor.h"
#include <cstring>
#include <functional>
#include <initializer_list>
#include <map>

#ifdef MMD_MCP_TEST
namespace mmp {
    struct Float3 {
        float x, y, z;
    };
    struct CameraKeyFrameData {
        int frame_no;
        int pre_index;
        int next_index;
        float length;
        Float3 xyz;
        Float3 rxyz;
        char hokan1_x[6];
        char hokan1_y[6];
        char hokan2_x[6];
        char hokan2_y[6];
        int is_perspective;
        int view_angle;
        int is_selected;
        int looking_model_index;
        int looking_bone_index;
    };
}

// KeyframeTraits specialization for test builds
#include "common/keyframe_traits.h"

namespace mmd_mcp {

template<>
struct KeyframeTraits<mmp::CameraKeyFrameData> {
    static int frameNo(const mmp::CameraKeyFrameData& kf) { return kf.frame_no; }
    static void setFrameNo(mmp::CameraKeyFrameData& kf, int f) { kf.frame_no = f; }
    static int preIndex(const mmp::CameraKeyFrameData& kf) { return kf.pre_index; }
    static void setPreIndex(mmp::CameraKeyFrameData& kf, int i) { kf.pre_index = i; }
    static int nextIndex(const mmp::CameraKeyFrameData& kf) { return kf.next_index; }
    static void setNextIndex(mmp::CameraKeyFrameData& kf, int i) { kf.next_index = i; }
};

} // namespace mmd_mcp
#endif

class MockCameraAccessor : public ICameraKeyframeAccessor {
public:
    void seed(int frame, const mmp::CameraKeyFrameData& data) {
        store_[frame] = data;
    }

    void forEachCameraKeyframe(
        const std::function<void(const mmp::CameraKeyFrameData&)>& visitor) const override {
        for (auto& [frame, kf] : store_) {
            visitor(kf);
        }
    }

    KeyframeResult createCameraKeyframe(int frame,
        const std::function<void(mmp::CameraKeyFrameData&)>& initializer) override {
        if (frame < 0 || frame >= mmd_mcp::MAX_CAMERA_FRAMES) return KeyframeResult::Failed;
        if (store_.count(frame)) return KeyframeResult::AlreadyExists;
        mmp::CameraKeyFrameData kf{};
        std::memset(&kf, 0, sizeof(kf));
        kf.frame_no = frame;
        initializer(kf);
        store_[frame] = kf;
        return KeyframeResult::Success;
    }

    KeyframeResult updateCameraKeyframe(int frame,
        const std::function<void(mmp::CameraKeyFrameData&)>& modifier) override {
        auto it = store_.find(frame);
        if (it == store_.end()) return KeyframeResult::NotFound;
        modifier(it->second);
        return KeyframeResult::Success;
    }

    KeyframeResult deleteCameraKeyframe(int frame) override {
        return store_.erase(frame) > 0 ? KeyframeResult::Success : KeyframeResult::NotFound;
    }

    const mmp::CameraKeyFrameData* get(int frame) const {
        auto it = store_.find(frame);
        return it != store_.end() ? &it->second : nullptr;
    }

    int size() const { return static_cast<int>(store_.size()); }
    int count() const { return size(); }

    // フレーム番号リストでストアを初期化
    void reset(std::initializer_list<int> frames) {
        store_.clear();
        for (int f : frames) {
            mmp::CameraKeyFrameData kf{};
            std::memset(&kf, 0, sizeof(kf));
            kf.frame_no = f;
            store_[f] = kf;
        }
    }

private:
    std::map<int, mmp::CameraKeyFrameData> store_;
};
