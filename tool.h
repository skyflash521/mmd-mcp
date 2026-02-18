#pragma once

#include <nlohmann/json.hpp>
#include <string>

class ITool {
public:
    virtual ~ITool() = default;
    virtual std::string name() const = 0;
    virtual std::string description() const = 0;
    virtual nlohmann::json inputSchema() const = 0;
    virtual nlohmann::json execute(const nlohmann::json& args) = 0;
};
