#pragma once

#include "tools/model/model_accessor.h"
#include <map>
#include <set>
#include <vector>

class MockModelAccessor : public IModelAccessor {
public:
    void seed(int index, const ModelInfo& info,
              const std::vector<BoneInfo>& bones = {},
              const std::vector<MorphBasicInfo>& morphs = {}) {
        models_[index] = info;
        bones_[index] = bones;
        morphs_[index] = morphs;
    }

    std::vector<std::pair<int, ModelInfo>> listModels() const override {
        std::vector<std::pair<int, ModelInfo>> result;
        for (auto& [index, info] : models_) {
            result.emplace_back(index, info);
        }
        return result;
    }

    bool getModelInfo(int index, ModelInfo& out) const override {
        auto it = models_.find(index);
        if (it == models_.end()) return false;
        out = it->second;
        return true;
    }

    std::vector<BoneInfo> getBones(int index) const override {
        auto it = bones_.find(index);
        if (it == bones_.end()) return {};
        return it->second;
    }

    void setMorphsFail(int index) { morphs_fail_.insert(index); }
    void setMorphsFileModified(int index) { morphs_file_modified_.insert(index); }

    std::pair<PmxStatus, std::vector<MorphBasicInfo>> getMorphs(int index) const override {
        if (morphs_file_modified_.count(index)) return {PmxStatus::file_modified_after_launch, {}};
        if (morphs_fail_.count(index)) return {PmxStatus::parse_failed, {}};
        auto it = morphs_.find(index);
        if (it == morphs_.end()) return {PmxStatus::ok, {}};
        return {PmxStatus::ok, it->second};
    }

private:
    std::map<int, ModelInfo> models_;
    std::map<int, std::vector<BoneInfo>> bones_;
    std::map<int, std::vector<MorphBasicInfo>> morphs_;
    std::set<int> morphs_fail_;
    std::set<int> morphs_file_modified_;
};
