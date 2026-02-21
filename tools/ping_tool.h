#pragma once

#include "tool.h"

class PingTool : public ITool {
public:
    std::string name() const override { return "ping"; }
    std::string description() const override { return "Check server connectivity"; }

    nlohmann::json inputSchema() const override {
        return {{"type", "object"}, {"properties", nlohmann::json::object()}, {"additionalProperties", false}};
    }

    nlohmann::json execute(const nlohmann::json&) override {
        return {
            {"content", nlohmann::json::array({{{"type", "text"}, {"text", "pong"}}})},
            {"isError", false}
        };
    }
};
