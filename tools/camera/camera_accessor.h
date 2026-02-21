#pragma once

#include <string>
#include <functional>

namespace mmd_mcp {
    static constexpr int MAX_CAMERA_FRAMES = 10000;
}

enum class KeyframeResult {
    Success,
    AlreadyExists,   // create: 指定フレームに既存キーフレームあり
    NotFound,         // update/delete: 指定フレームにキーフレームなし
    Failed            // UI操作失敗、データ不整合など
};

namespace mmp {
    struct CameraKeyFrameData;
}

class ICameraKeyframeAccessor {
public:
    virtual ~ICameraKeyframeAccessor() = default;

    // 読み取り
    virtual void forEachCameraKeyframe(
        const std::function<void(const mmp::CameraKeyFrameData&)>& visitor) const = 0;

    // 作成: AlreadyExists=既存あり, Failed=操作失敗, Success=成功
    virtual KeyframeResult createCameraKeyframe(
        int frame, const std::function<void(mmp::CameraKeyFrameData&)>& initializer) = 0;

    // 更新: NotFound=キーフレームなし, Failed=操作失敗, Success=成功
    virtual KeyframeResult updateCameraKeyframe(
        int frame, const std::function<void(mmp::CameraKeyFrameData&)>& modifier) = 0;

    // 削除: NotFound=キーフレームなし, Failed=操作失敗, Success=成功
    virtual KeyframeResult deleteCameraKeyframe(int frame) = 0;
};

#ifndef MMD_MCP_TEST
#include "mmd_plugin.h"
#include "common/keyframe_traits.h"
#include "common/keyframe_linked_list.h"

namespace mmd_mcp {

// MMD UIコントロールID
static constexpr int FRAME_EDIT_ID = 0x1A1;
static constexpr int CAMERA_REGISTER_BUTTON_ID = 0x1C4;
static constexpr int TIMELINE_DELETE_BUTTON_ID = 0x1A7;
static constexpr int TIMELINE_HSCROLL_ID = 0x1AC;

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

class MmdCameraAccessor : public ICameraKeyframeAccessor {
    using KF = mmp::CameraKeyFrameData;
    using Traits = mmd_mcp::KeyframeTraits<KF>;

public:
    void forEachCameraKeyframe(
        const std::function<void(const KF&)>& visitor) const override {
        auto* data = mmp::getMMDMainData();
        if (!data) return;
        mmd_mcp::forEachKeyframe<KF, Traits>(
            &data->camera_key_frame[0], mmd_mcp::MAX_CAMERA_FRAMES, visitor);
    }

    KeyframeResult createCameraKeyframe(int frame,
        const std::function<void(KF&)>& initializer) override {
        auto* data = mmp::getMMDMainData();
        if (!data) return KeyframeResult::Failed;
        auto* arr = &data->camera_key_frame[0];

        // 既存チェック
        if (mmd_mcp::findKeyframeIndex<KF, Traits>(arr, mmd_mcp::MAX_CAMERA_FRAMES, frame) >= 0)
            return KeyframeResult::AlreadyExists;

        // MMD UIを操作してキーフレームを登録
        HWND hwnd = ::getHWND();
        if (!hwnd) return KeyframeResult::Failed;

        // 1. 目的フレームに移動
        HWND editHwnd = GetDlgItem(hwnd, mmd_mcp::FRAME_EDIT_ID);
        if (!editHwnd) return KeyframeResult::Failed;
        std::string frameStr = std::to_string(frame);
        SetWindowTextA(editHwnd, frameStr.c_str());
        SendMessage(editHwnd, WM_KEYDOWN, VK_RETURN, 0);
        SendMessage(editHwnd, WM_KEYUP, VK_RETURN, 0);

        // 2. カメラ登録ボタンをクリック
        HWND registerBtn = GetDlgItem(hwnd, mmd_mcp::CAMERA_REGISTER_BUTTON_ID);
        if (!registerBtn) return KeyframeResult::Failed;
        SendMessage(registerBtn, BM_CLICK, 0, 0);

        // 3. MMDが作成したキーフレームを探してデータを上書き
        int idx = mmd_mcp::findKeyframeIndex<KF, Traits>(arr, mmd_mcp::MAX_CAMERA_FRAMES, frame);
        if (idx < 0) return KeyframeResult::Failed;
        initializer(arr[idx]);
        refreshTimeline();
        return KeyframeResult::Success;
    }

    KeyframeResult updateCameraKeyframe(int frame,
        const std::function<void(KF&)>& modifier) override {
        auto* data = mmp::getMMDMainData();
        if (!data) return KeyframeResult::Failed;
        auto* arr = &data->camera_key_frame[0];

        // 既存チェック
        if (mmd_mcp::findKeyframeIndex<KF, Traits>(arr, mmd_mcp::MAX_CAMERA_FRAMES, frame) < 0)
            return KeyframeResult::NotFound;

        // MMD UIを操作してキーフレームを登録（上書き）
        HWND hwnd = ::getHWND();
        if (!hwnd) return KeyframeResult::Failed;

        // 1. 目的フレームに移動
        HWND editHwnd = GetDlgItem(hwnd, mmd_mcp::FRAME_EDIT_ID);
        if (!editHwnd) return KeyframeResult::Failed;
        std::string frameStr = std::to_string(frame);
        SetWindowTextA(editHwnd, frameStr.c_str());
        SendMessage(editHwnd, WM_KEYDOWN, VK_RETURN, 0);
        SendMessage(editHwnd, WM_KEYUP, VK_RETURN, 0);

        // 2. カメラ登録ボタンをクリック
        HWND registerBtn = GetDlgItem(hwnd, mmd_mcp::CAMERA_REGISTER_BUTTON_ID);
        if (!registerBtn) return KeyframeResult::Failed;
        SendMessage(registerBtn, BM_CLICK, 0, 0);

        // 3. キーフレームのデータを上書き
        int idx = mmd_mcp::findKeyframeIndex<KF, Traits>(arr, mmd_mcp::MAX_CAMERA_FRAMES, frame);
        if (idx < 0) return KeyframeResult::Failed;
        modifier(arr[idx]);
        refreshTimeline();
        return KeyframeResult::Success;
    }

    KeyframeResult deleteCameraKeyframe(int frame) override {
        if (frame == 0) return KeyframeResult::NotFound;
        auto* data = mmp::getMMDMainData();
        if (!data) return KeyframeResult::Failed;
        auto* arr = &data->camera_key_frame[0];

        // 削除対象の存在確認
        int idx = mmd_mcp::findKeyframeIndex<KF, Traits>(arr, mmd_mcp::MAX_CAMERA_FRAMES, frame);
        if (idx < 0) return KeyframeResult::NotFound;

        HWND hwnd = ::getHWND();
        if (!hwnd) return KeyframeResult::Failed;

        // 1. 目的フレームに移動
        HWND editHwnd = GetDlgItem(hwnd, mmd_mcp::FRAME_EDIT_ID);
        if (!editHwnd) return KeyframeResult::Failed;
        std::string frameStr = std::to_string(frame);
        SetWindowTextA(editHwnd, frameStr.c_str());
        SendMessage(editHwnd, WM_KEYDOWN, VK_RETURN, 0);
        SendMessage(editHwnd, WM_KEYUP, VK_RETURN, 0);

        // 2. キーフレームを選択状態にする
        arr[idx].is_selected = 1;

        // 3. タイムライン削除ボタンをクリック
        HWND deleteBtn = GetDlgItem(hwnd, mmd_mcp::TIMELINE_DELETE_BUTTON_ID);
        if (!deleteBtn) return KeyframeResult::Failed;
        SendMessage(deleteBtn, BM_CLICK, 0, 0);

        // 4. 削除確認
        int checkIdx = mmd_mcp::findKeyframeIndex<KF, Traits>(arr, mmd_mcp::MAX_CAMERA_FRAMES, frame);
        return checkIdx < 0 ? KeyframeResult::Success : KeyframeResult::Failed;
    }

private:
    // メモリ書き換え後にタイムラインの表示を更新する
    static void refreshTimeline() {
        // TODO: 効果のある再描画方法を調査・実装する
    }
};
#endif
