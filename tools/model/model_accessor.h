#pragma once

#include <string>
#include <vector>
#include <utility>

struct ModelInfo {
    std::string name_jp;
    std::string name_en;
    std::string comment_jp;
    std::string file_path;    // UTF-8
    int bone_count = 0;
    int morph_count = 0;
    int ik_count = 0;
    bool is_visible = false;
};

struct BoneInfo {
    std::string name_jp;
    std::string name_en;
};

struct MorphBasicInfo {
    std::string name_jp;
    std::string name_en;
    int panel = 0;  // 0:システム予約, 1:眉, 2:目, 3:口, 4:その他
    int type = 0;   // 0:Group, 1:Vertex, 2:Bone, 3-7:UV, 8:Material, 9:Flip, 10:Impulse
};

enum class PmxStatus {
    ok,
    parse_failed,
    file_modified_after_launch
};

class IModelAccessor {
public:
    virtual ~IModelAccessor() = default;

    // index付きモデル概要の一覧を返す
    virtual std::vector<std::pair<int, ModelInfo>> listModels() const = 0;

    // 指定インデックスのモデル情報を返す。見つからなければ false
    virtual bool getModelInfo(int index, ModelInfo& out) const = 0;

    // 指定インデックスのモデルのボーン名一覧を返す
    virtual std::vector<BoneInfo> getBones(int index) const = 0;

    // 指定インデックスのモデルのモーフ一覧を返す（PMXファイルから取得）
    virtual std::pair<PmxStatus, std::vector<MorphBasicInfo>> getMorphs(int index) const = 0;
};

#ifndef MMD_MCP_TEST
#include "mmd_plugin.h"
#include "common/encoding.h"
#include "common/pmx_parser.h"
#include <mutex>
#include <map>

class MmdModelAccessor : public IModelAccessor {
    static constexpr int MAX_MODELS = 255;

    struct CachedMorphs {
        bool ok = false;
        std::vector<MorphBasicInfo> morphs;
    };

public:
    MmdModelAccessor() {
        FILETIME creation, exit, kernel, user;
        if (GetProcessTimes(GetCurrentProcess(), &creation, &exit, &kernel, &user)) {
            process_start_time_ = creation;
        } else {
            process_start_time_.dwLowDateTime = 0;
            process_start_time_.dwHighDateTime = 0;
        }
    }

    std::vector<std::pair<int, ModelInfo>> listModels() const override {
        std::vector<std::pair<int, ModelInfo>> result;
        auto* data = mmp::getMMDMainData();
        if (!data) return result;

        for (int i = 0; i < MAX_MODELS; ++i) {
            auto* model = data->model_data[i];
            if (!model) continue;

            ModelInfo info;
            info.name_jp = mmdToUtf8(model->name_jp);
            info.name_en = mmdToUtf8(model->name_en);
            info.bone_count = model->bone_count;
            info.morph_count = model->morph_count;
            info.is_visible = model->is_visible != 0;
            result.emplace_back(i, std::move(info));
        }
        return result;
    }

    bool getModelInfo(int index, ModelInfo& out) const override {
        if (index < 0 || index >= MAX_MODELS) return false;
        auto* data = mmp::getMMDMainData();
        if (!data) return false;
        auto* model = data->model_data[index];
        if (!model) return false;

        out.name_jp = mmdToUtf8(model->name_jp);
        out.name_en = mmdToUtf8(model->name_en);
        out.comment_jp = mmdToUtf8(model->comment_jp);
        out.file_path = wcharToUtf8(model->file_path);
        out.bone_count = model->bone_count;
        out.morph_count = model->morph_count;
        out.ik_count = model->ik_count;
        out.is_visible = model->is_visible != 0;
        return true;
    }

    std::vector<BoneInfo> getBones(int index) const override {
        std::vector<BoneInfo> result;
        if (index < 0 || index >= MAX_MODELS) return result;
        auto* data = mmp::getMMDMainData();
        if (!data) return result;
        auto* model = data->model_data[index];
        if (!model || !model->bone_current_data) return result;

        result.reserve(model->bone_count);
        for (int i = 0; i < model->bone_count; ++i) {
            auto& bone = model->bone_current_data[i];
            result.push_back({mmdToUtf8(bone.name_jp), mmdToUtf8(bone.name_en)});
        }
        return result;
    }

    std::pair<PmxStatus, std::vector<MorphBasicInfo>> getMorphs(int index) const override {
        if (index < 0 || index >= MAX_MODELS) return {PmxStatus::parse_failed, {}};
        auto* data = mmp::getMMDMainData();
        if (!data) return {PmxStatus::parse_failed, {}};
        auto* model = data->model_data[index];
        if (!model) return {PmxStatus::parse_failed, {}};

        std::wstring filePath(model->file_path ? model->file_path : L"");

        // PMXファイルがMMD起動後に更新されていないか確認
        if (!filePath.empty() && isFileModifiedAfterLaunch(filePath)) {
            return {PmxStatus::file_modified_after_launch, {}};
        }

        std::lock_guard<std::mutex> lock(cache_mutex_);
        auto it = morph_cache_.find(filePath);
        if (it != morph_cache_.end()) {
            return {it->second.ok ? PmxStatus::ok : PmxStatus::parse_failed, it->second.morphs};
        }

        auto parsed = parsePmxMorphs(filePath);
        morph_cache_[filePath] = parsed;
        return {parsed.ok ? PmxStatus::ok : PmxStatus::parse_failed, parsed.morphs};
    }

protected:
    virtual CachedMorphs parsePmxMorphs(const std::wstring& filePath) const {
        CachedMorphs cached;
        auto pmxInfo = pmx::parse(filePath.c_str());
        if (!pmxInfo.valid) return cached;

        cached.ok = true;
        cached.morphs.reserve(pmxInfo.morphs.size());
        for (auto& m : pmxInfo.morphs) {
            cached.morphs.push_back({std::move(m.name_jp), std::move(m.name_en), m.panel, m.type});
        }
        return cached;
    }

private:
    bool isFileModifiedAfterLaunch(const std::wstring& filePath) const {
        if (process_start_time_.dwLowDateTime == 0 && process_start_time_.dwHighDateTime == 0)
            return false;  // プロセス時刻取得失敗時は判定スキップ
        WIN32_FILE_ATTRIBUTE_DATA fileAttr;
        if (!GetFileAttributesExW(filePath.c_str(), GetFileExInfoStandard, &fileAttr))
            return false;  // ファイル情報取得失敗時は判定スキップ
        return CompareFileTime(&fileAttr.ftLastWriteTime, &process_start_time_) > 0;
    }

    static std::string wcharToUtf8(const wchar_t* wstr) {
        if (!wstr || !wstr[0]) return {};
        int len = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
        if (len <= 0) return {};
        std::string result(len - 1, '\0');
        WideCharToMultiByte(CP_UTF8, 0, wstr, -1, &result[0], len, nullptr, nullptr);
        return result;
    }

    FILETIME process_start_time_;
    mutable std::mutex cache_mutex_;
    mutable std::map<std::wstring, CachedMorphs> morph_cache_;
};
#endif
