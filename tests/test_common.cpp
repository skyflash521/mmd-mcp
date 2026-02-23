#include "tests/test_common_setup.h"
#include "tests/mock_camera.h"
#include "common/frame_set.h"
#include "common/keyframe_linked_list.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <functional>
#include <set>
#include <stdexcept>
#include <vector>

static int g_passed = 0;

using KF = mmp::CameraKeyFrameData;
using Traits = mmd_mcp::KeyframeTraits<KF>;

// --- FrameSet tests ---

static void test_parse_single() {
    auto fs = FrameSet::parse("5");
    assert(fs.contains(5));
    assert(!fs.contains(4));
    assert(!fs.contains(6));
    ++g_passed; printf("  PASS: FrameSet single\n");
}

static void test_parse_range() {
    auto fs = FrameSet::parse("1-5");
    assert(!fs.contains(0));
    assert(fs.contains(1));
    assert(fs.contains(3));
    assert(fs.contains(5));
    assert(!fs.contains(6));
    ++g_passed; printf("  PASS: FrameSet range\n");
}

static void test_parse_complex() {
    auto fs = FrameSet::parse("1-3,5,8-10");
    assert(fs.contains(1));
    assert(fs.contains(3));
    assert(!fs.contains(4));
    assert(fs.contains(5));
    assert(!fs.contains(7));
    assert(fs.contains(8));
    assert(fs.contains(10));
    assert(!fs.contains(11));
    ++g_passed; printf("  PASS: FrameSet complex\n");
}

static void test_parse_error_open_end() {
    bool caught = false;
    try { FrameSet::parse("98-"); } catch (const std::invalid_argument&) { caught = true; }
    assert(caught);
    ++g_passed; printf("  PASS: FrameSet error open end\n");
}

static void test_parse_error_open_start() {
    bool caught = false;
    try { FrameSet::parse("-2"); } catch (const std::invalid_argument&) { caught = true; }
    assert(caught);
    ++g_passed; printf("  PASS: FrameSet error open start\n");
}

static void test_parse_overlap() {
    auto fs = FrameSet::parse("1-3,2-4");
    assert(fs.contains(1));
    assert(fs.contains(2));
    assert(fs.contains(3));
    assert(fs.contains(4));
    assert(!fs.contains(5));
    ++g_passed; printf("  PASS: FrameSet overlap\n");
}

static void test_parse_error_spaces() {
    bool caught = false;
    try { FrameSet::parse(" 1 , 3 - 5 "); } catch (const std::invalid_argument&) { caught = true; }
    assert(caught);
    ++g_passed; printf("  PASS: FrameSet error spaces\n");
}

static void test_parse_empty() {
    auto fs = FrameSet::parse("");
    assert(fs.empty());
    assert(!fs.contains(0));
    ++g_passed; printf("  PASS: FrameSet empty\n");
}

static void test_parse_error_alpha() {
    bool caught = false;
    try { FrameSet::parse("abc"); } catch (const std::invalid_argument&) { caught = true; }
    assert(caught);
    ++g_passed; printf("  PASS: FrameSet error alpha\n");
}

static void test_parse_error_multi_dash() {
    bool caught = false;
    try { FrameSet::parse("1-2-3"); } catch (const std::invalid_argument&) { caught = true; }
    assert(caught);
    ++g_passed; printf("  PASS: FrameSet error multi dash\n");
}

static void test_parse_error_bare_dash() {
    bool caught = false;
    try { FrameSet::parse("-"); } catch (const std::invalid_argument&) { caught = true; }
    assert(caught);
    ++g_passed; printf("  PASS: FrameSet error bare dash\n");
}

static void test_parse_error_reversed_range() {
    // 逆順レンジ "5-1" はエラー
    bool caught = false;
    try { FrameSet::parse("5-1"); } catch (const std::invalid_argument&) { caught = true; }
    assert(caught);
    ++g_passed; printf("  PASS: FrameSet error reversed range\n");
}


static void test_parse_consecutive_commas() {
    // "1,,3" — 空トークンはスキップされる
    auto fs = FrameSet::parse("1,,3");
    assert(fs.contains(1));
    assert(!fs.contains(2));
    assert(fs.contains(3));
    ++g_passed; printf("  PASS: FrameSet consecutive commas\n");
}

static void test_parse_error_overflow() {
    // int の範囲を超える値 → invalid_argument
    bool caught = false;
    try { FrameSet::parse("99999999999"); } catch (const std::invalid_argument&) { caught = true; }
    assert(caught);
    ++g_passed; printf("  PASS: FrameSet error overflow\n");
}

static void test_parse_error_overflow_in_range() {
    // レンジの端がオーバーフロー
    bool caught = false;
    try { FrameSet::parse("1-99999999999"); } catch (const std::invalid_argument&) { caught = true; }
    assert(caught);
    ++g_passed; printf("  PASS: FrameSet error overflow in range\n");
}

// --- keyframe_linked_list テスト ---

// テスト用配列サイズ
static constexpr int TEST_ARR_SIZE = 10;

// MSVCのassertマクロがテンプレート引数のカンマを誤解釈する問題を回避するラッパー
static void kfForEach(KF* arr, const std::function<void(const KF&)>& v) {
    mmd_mcp::forEachKeyframe<KF, Traits>(arr, TEST_ARR_SIZE, v);
}
static int kfCount(KF* arr) { return mmd_mcp::countKeyframes<KF, Traits>(arr, TEST_ARR_SIZE); }
static int kfFind(KF* arr, int frame) { return mmd_mcp::findKeyframeIndex<KF, Traits>(arr, TEST_ARR_SIZE, frame); }

// 配列にリンクリストを構築するヘルパー
// フレーム番号昇順、インデックスは配列位置に一致
static void initLinkedList(KF* arr, int count, const int* frameNos) {
    for (int i = 0; i < count; ++i) {
        std::memset(&arr[i], 0, sizeof(KF));
        Traits::setFrameNo(arr[i], frameNos[i]);
        Traits::setPreIndex(arr[i], (i > 0) ? i - 1 : 0);
        Traits::setNextIndex(arr[i], (i < count - 1) ? i + 1 : 0);
    }
}

static void test_forEach() {
    KF arr[10] = {};
    int frames[] = {0, 10, 20};
    initLinkedList(arr, 3, frames);

    int visited = 0;
    int lastFrame = -1;
    kfForEach(arr, [&](const KF& kf) {
        assert(Traits::frameNo(kf) > lastFrame || visited == 0);
        lastFrame = Traits::frameNo(kf);
        ++visited;
    });
    assert(visited == 3);
    ++g_passed; printf("  PASS: forEachKeyframe\n");
}

static void test_count() {
    KF arr[10] = {};
    int frames[] = {0, 5, 10, 15};
    initLinkedList(arr, 4, frames);

    assert(kfCount(arr) == 4);
    ++g_passed; printf("  PASS: countKeyframes\n");
}

static void test_find() {
    KF arr[10] = {};
    int frames[] = {0, 10, 20, 30};
    initLinkedList(arr, 4, frames);

    assert(kfFind(arr,0) == 0);
    assert(kfFind(arr,20) == 2);
    assert(kfFind(arr,30) == 3);
    assert(kfFind(arr,15) == -1);
    assert(kfFind(arr,99) == -1);
    ++g_passed; printf("  PASS: findKeyframeIndex\n");
}

static void test_null_array() {
    // nullポインタに対する各関数の安全性
    assert(kfCount(nullptr) == 0);
    assert(kfFind(nullptr, 0) == -1);

    int visited = 0;
    kfForEach(nullptr, [&](const KF&) { ++visited; });
    assert(visited == 0);
    ++g_passed; printf("  PASS: null array safety\n");
}

// --- 破損リンク耐性テスト ---

static void test_corrupted_cycle() {
    // next_index が自分自身を指す循環破損
    KF arr[10] = {};
    std::memset(arr, 0, sizeof(arr));
    Traits::setFrameNo(arr[0], 0);
    Traits::setNextIndex(arr[0], 1);
    Traits::setFrameNo(arr[1], 10);
    Traits::setNextIndex(arr[1], 1);  // 自己循環

    // forEach がハングしないこと
    int visited = 0;
    kfForEach(arr, [&](const KF&) { ++visited; });
    assert(visited >= 1 && visited <= TEST_ARR_SIZE + 1);

    // count がハングしないこと
    int c = kfCount(arr);
    assert(c >= 1);

    // find がハングしないこと
    int idx = kfFind(arr, 999);
    assert(idx == -1);  // 見つからないはず

    ++g_passed; printf("  PASS: corrupted cycle (self-loop) guard\n");
}

static void test_corrupted_out_of_bounds() {
    // next_index が配列外を指す
    KF arr[10] = {};
    std::memset(arr, 0, sizeof(arr));
    Traits::setFrameNo(arr[0], 0);
    Traits::setNextIndex(arr[0], 999);  // 配列外

    int visited = 0;
    kfForEach(arr, [&](const KF&) { ++visited; });
    assert(visited == 1);  // 先頭のみ

    assert(kfCount(arr) == 1);
    assert(kfFind(arr, 0) == 0);
    assert(kfFind(arr, 10) == -1);

    ++g_passed; printf("  PASS: corrupted out-of-bounds index guard\n");
}

static void test_corrupted_negative_index() {
    // next_index が負値
    KF arr[10] = {};
    std::memset(arr, 0, sizeof(arr));
    Traits::setFrameNo(arr[0], 0);
    Traits::setNextIndex(arr[0], -5);

    int visited = 0;
    kfForEach(arr, [&](const KF&) { ++visited; });
    assert(visited == 1);

    assert(kfCount(arr) == 1);

    ++g_passed; printf("  PASS: corrupted negative index guard\n");
}

// --- forEachKeyframe startIndex テスト ---

// 共有プール型リンクリストを構築するヘルパー
// entries: {startIndex, {frame0, frame1, ...}} の組
// 各リストの先頭は arr[startIndex]、以降は freeSlot から空きスロットを確保して連結
static void initSharedPool(KF* arr, int arrSize,
    const std::vector<std::pair<int, std::vector<int>>>& entries) {
    std::memset(arr, 0, sizeof(KF) * arrSize);
    int freeSlot = 0;
    // freeSlot が entries のヘッドインデックスと衝突しないよう、使用済みを飛ばす
    std::set<int> headIndices;
    for (auto& [start, _] : entries) headIndices.insert(start);

    auto nextFree = [&]() {
        while (headIndices.count(freeSlot)) ++freeSlot;
        assert(freeSlot < arrSize && "initSharedPool: pool exhausted");
        return freeSlot++;
    };

    for (auto& [start, frames] : entries) {
        int prev = start;
        Traits::setFrameNo(arr[start], frames[0]);
        Traits::setPreIndex(arr[start], start);
        Traits::setNextIndex(arr[start], start); // 自己ループ（後で上書き）

        for (size_t i = 1; i < frames.size(); ++i) {
            int slot = nextFree();
            Traits::setFrameNo(arr[slot], frames[i]);
            Traits::setPreIndex(arr[slot], prev);
            Traits::setNextIndex(arr[prev], slot);
            Traits::setNextIndex(arr[slot], start); // 暫定で先頭に戻す
            prev = slot;
        }
    }
}

static void kfForEachFrom(KF* arr, const std::function<void(const KF&)>& v, int startIndex) {
    mmd_mcp::forEachKeyframe<KF, Traits>(arr, TEST_ARR_SIZE, v, startIndex);
}

static void test_startIndex_normal() {
    // 2つのモーフが共有プールにある場合をシミュレート
    // morph 0: head=0, frames=[0, 10, 20]
    // morph 1: head=1, frames=[0, 30]
    KF arr[10] = {};
    initSharedPool(arr, 10, {
        {0, {0, 10, 20}},
        {1, {0, 30}}
    });

    // morph 0 走査
    std::vector<int> frames0;
    kfForEachFrom(arr, [&](const KF& kf) { frames0.push_back(Traits::frameNo(kf)); }, 0);
    assert(frames0.size() == 3);
    assert(frames0[0] == 0);
    assert(frames0[1] == 10);
    assert(frames0[2] == 20);

    // morph 1 走査
    std::vector<int> frames1;
    kfForEachFrom(arr, [&](const KF& kf) { frames1.push_back(Traits::frameNo(kf)); }, 1);
    assert(frames1.size() == 2);
    assert(frames1[0] == 0);
    assert(frames1[1] == 30);

    ++g_passed; printf("  PASS: forEachKeyframe startIndex normal\n");
}

static void test_startIndex_sentinel_only() {
    // morph のヘッドが自己ループ（キーフレーム無し）
    KF arr[10] = {};
    std::memset(arr, 0, sizeof(arr));
    Traits::setFrameNo(arr[3], 0);
    Traits::setNextIndex(arr[3], 3); // 自分に戻る

    std::vector<int> frames;
    kfForEachFrom(arr, [&](const KF& kf) { frames.push_back(Traits::frameNo(kf)); }, 3);
    assert(frames.size() == 1);
    assert(frames[0] == 0);

    ++g_passed; printf("  PASS: forEachKeyframe startIndex sentinel only\n");
}

static void test_startIndex_out_of_range() {
    KF arr[10] = {};
    std::memset(arr, 0, sizeof(arr));

    // startIndex が範囲外 → 何も走査しない
    int visited = 0;
    kfForEachFrom(arr, [&](const KF&) { ++visited; }, 10);
    assert(visited == 0);

    kfForEachFrom(arr, [&](const KF&) { ++visited; }, -1);
    assert(visited == 0);

    ++g_passed; printf("  PASS: forEachKeyframe startIndex out of range\n");
}

static void test_startIndex_corrupted_link() {
    // startIndex からの連結リストが配列外を指す
    KF arr[10] = {};
    std::memset(arr, 0, sizeof(arr));
    Traits::setFrameNo(arr[5], 0);
    Traits::setNextIndex(arr[5], 999); // 配列外

    std::vector<int> frames;
    kfForEachFrom(arr, [&](const KF& kf) { frames.push_back(Traits::frameNo(kf)); }, 5);
    assert(frames.size() == 1); // 先頭のみ

    ++g_passed; printf("  PASS: forEachKeyframe startIndex corrupted link\n");
}

int main() {
    suppressWindowsDialogs();
    printf("Running common tests...\n");

    // FrameSet
    test_parse_single();
    test_parse_range();
    test_parse_complex();
    test_parse_error_open_end();
    test_parse_error_open_start();
    test_parse_overlap();
    test_parse_empty();
    test_parse_error_alpha();
    test_parse_error_multi_dash();
    test_parse_error_bare_dash();
    test_parse_error_spaces();
    test_parse_error_reversed_range();
    test_parse_consecutive_commas();
    test_parse_error_overflow();
    test_parse_error_overflow_in_range();

    // keyframe_linked_list
    test_forEach();
    test_count();
    test_find();
    test_null_array();

    // 破損リンク耐性
    test_corrupted_cycle();
    test_corrupted_out_of_bounds();
    test_corrupted_negative_index();

    // startIndex 対応
    test_startIndex_normal();
    test_startIndex_sentinel_only();
    test_startIndex_out_of_range();
    test_startIndex_corrupted_link();

    printf("All %d common tests passed.\n", g_passed);
    return 0;
}
