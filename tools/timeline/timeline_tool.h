#pragma once

#include "tool.h"
#include "tools/timeline/timeline_accessor.h"
#include <string>

class GetCurrentFrameTool : public ITool {
public:
    explicit GetCurrentFrameTool(ICurrentFrameReader* reader) : reader_(reader) {}

    std::string name() const override { return "get_current_frame"; }
    std::string description() const override { return "Get the current frame number"; }

    nlohmann::json inputSchema() const override {
        return {{"type", "object"}, {"properties", nlohmann::json::object()}, {"additionalProperties", false}};
    }

    nlohmann::json execute(const nlohmann::json&) override {
        if (!reader_) {
            return {
                {"content", nlohmann::json::array({{{"type", "text"}, {"text", "MMD data not available"}}})},
                {"isError", true}
            };
        }
        int frame = reader_->getFrame();
        if (frame < 0) {
            return {
                {"content", nlohmann::json::array({{{"type", "text"}, {"text", "Failed to get frame"}}})},
                {"isError", true}
            };
        }
        return {
            {"content", nlohmann::json::array({{{"type", "text"}, {"text", std::to_string(frame)}}})},
            {"isError", false}
        };
    }

private:
    ICurrentFrameReader* reader_;
};

class SetCurrentFrameTool : public ITool {
public:
    explicit SetCurrentFrameTool(ICurrentFrameWriter* writer) : writer_(writer) {}

    std::string name() const override { return "set_current_frame"; }
    std::string description() const override { return "Set the current frame number"; }

    nlohmann::json inputSchema() const override {
        return {
            {"type", "object"},
            {"properties", {
                {"frame", {{"type", "integer"}, {"description", "Frame number to set"}, {"minimum", 0}}}
            }},
            {"required", nlohmann::json::array({"frame"})}
        };
    }

    nlohmann::json execute(const nlohmann::json& args) override {
        if (!writer_) {
            return {
                {"content", nlohmann::json::array({{{"type", "text"}, {"text", "MMD data not available"}}})},
                {"isError", true}
            };
        }
        if (!args.contains("frame") || !args["frame"].is_number_integer()) {
            return {
                {"content", nlohmann::json::array({{{"type", "text"}, {"text", "Missing or invalid 'frame' argument"}}})},
                {"isError", true}
            };
        }
        int frame = args["frame"].get<int>();
        if (frame < 0) {
            return {
                {"content", nlohmann::json::array({{{"type", "text"}, {"text", "Frame must be non-negative"}}})},
                {"isError", true}
            };
        }
        writer_->setFrame(frame);
        return {
            {"content", nlohmann::json::array({{{"type", "text"}, {"text", "Frame set to " + std::to_string(frame)}}})},
            {"isError", false}
        };
    }

private:
    ICurrentFrameWriter* writer_;
};
