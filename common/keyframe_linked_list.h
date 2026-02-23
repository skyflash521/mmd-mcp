#pragma once

#include "common/keyframe_traits.h"
#include <functional>

namespace mmd_mcp {

// MMDのキーフレーム配列を走査するための読み取り専用ユーティリティ。
// キーフレームは固定長配列 [0, maxCount) に配置され、
// index 0 起点の連結リストとして管理される。
// 全走査関数は maxCount による配列境界チェックと走査回数ガードにより、
// 破損リンクによる配列外アクセスや無限ループを防止する。

// MMDのキーフレーム配列は共有プール型の連結リスト。
// インデックス0は常にリストの起点（センチネル）であり、
// startIndex に戻った場合（循環）、またはインデックス0に到達した場合（別リストの終端）で停止する。
template<typename KF, typename Traits = KeyframeTraits<KF>>
void forEachKeyframe(KF* arr, int maxCount, const std::function<void(const KF&)>& visitor, int startIndex = 0) {
    if (!arr || maxCount <= 0) return;
    if (startIndex < 0 || startIndex >= maxCount) return;
    int idx = startIndex;
    int guard = 0;
    do {
        visitor(arr[idx]);
        idx = Traits::nextIndex(arr[idx]);
        if (idx < 0 || idx >= maxCount) break;
        if (++guard >= maxCount) break;
        if (idx == startIndex) break;
        if (idx == 0 && startIndex != 0) break;
    } while (true);
}

template<typename KF, typename Traits = KeyframeTraits<KF>>
int countKeyframes(KF* arr, int maxCount) {
    if (!arr || maxCount <= 0) return 0;
    int count = 0;
    int idx = 0;
    do {
        ++count;
        idx = Traits::nextIndex(arr[idx]);
        if (idx < 0 || idx >= maxCount) break;
        if (count >= maxCount) break;
    } while (idx != 0);
    return count;
}

// 指定フレーム番号のキーフレームの配列インデックスを返す。見つからなければ -1。
template<typename KF, typename Traits = KeyframeTraits<KF>>
int findKeyframeIndex(KF* arr, int maxCount, int frame) {
    if (!arr || maxCount <= 0) return -1;
    int idx = 0;
    int guard = 0;
    do {
        if (Traits::frameNo(arr[idx]) == frame) return idx;
        idx = Traits::nextIndex(arr[idx]);
        if (idx < 0 || idx >= maxCount) break;
        if (++guard >= maxCount) break;
    } while (idx != 0);
    return -1;
}

} // namespace mmd_mcp
