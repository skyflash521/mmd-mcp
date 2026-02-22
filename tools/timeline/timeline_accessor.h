#pragma once

class ICurrentFrameReader {
public:
    virtual ~ICurrentFrameReader() = default;
    virtual int getFrame() const = 0;
};

class ICurrentFrameWriter {
public:
    virtual ~ICurrentFrameWriter() = default;
    virtual void setFrame(int frame) = 0;
};

#ifndef MMD_MCP_TEST
#include "mmd_plugin.h"
#include <string>

class MmdTimelineAccessor : public ICurrentFrameReader, public ICurrentFrameWriter {
public:
    int getFrame() const override {
        auto* data = mmp::getMMDMainData();
        if (!data) return -1;
        return data->now_frame;
    }

    void setFrame(int frame) override {
        HWND hwnd = ::getHWND();
        if (!hwnd) return;

        HWND editHwnd = GetDlgItem(hwnd, 0x1A1);
        if (!editHwnd) return;

        std::string frameStr = std::to_string(frame);
        SetWindowTextA(editHwnd, frameStr.c_str());
        SendMessage(editHwnd, WM_KEYDOWN, VK_RETURN, 0);
        SendMessage(editHwnd, WM_KEYUP, VK_RETURN, 0);

        OutputDebugStringA(("mmd_mcp: setFrame=" + frameStr + "\n").c_str());
    }
};
#endif
