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

class IModelAccessor {
public:
    virtual ~IModelAccessor() = default;

    // index付きモデル概要の一覧を返す
    virtual std::vector<std::pair<int, ModelInfo>> listModels() const = 0;

    // 指定インデックスのモデル情報を返す。見つからなければ false
    virtual bool getModelInfo(int index, ModelInfo& out) const = 0;

    // 指定インデックスのモデルのボーン名一覧を返す
    virtual std::vector<BoneInfo> getBones(int index) const = 0;
};

#ifndef MMD_MCP_TEST
#include "mmd_plugin.h"
#include "common/encoding.h"

class MmdModelAccessor : public IModelAccessor {
    static constexpr int MAX_MODELS = 255;

public:
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

private:
    static std::string wcharToUtf8(const wchar_t* wstr) {
        if (!wstr || !wstr[0]) return {};
        int len = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
        if (len <= 0) return {};
        std::string result(len - 1, '\0');
        WideCharToMultiByte(CP_UTF8, 0, wstr, -1, &result[0], len, nullptr, nullptr);
        return result;
    }
};
#endif
