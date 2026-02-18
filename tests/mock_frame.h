#pragma once

#include "tools/frame/frame_accessor.h"

class MockFrameReader : public IFrameReader {
    int frame_ = 0;
public:
    void set(int f) { frame_ = f; }
    int getFrame() const override { return frame_; }
};

class MockFrameWriter : public IFrameWriter {
    int frame_ = -1;
public:
    int written() const { return frame_; }
    void setFrame(int frame) override { frame_ = frame; }
};
