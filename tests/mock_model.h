#pragma once

#include "tools/model/model_accessor.h"
#include <map>
#include <vector>

class MockModelAccessor : public IModelAccessor {
public:
    void seed(int index, const ModelInfo& info, const std::vector<BoneInfo>& bones = {}) {
        models_[index] = info;
        bones_[index] = bones;
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

private:
    std::map<int, ModelInfo> models_;
    std::map<int, std::vector<BoneInfo>> bones_;
};
