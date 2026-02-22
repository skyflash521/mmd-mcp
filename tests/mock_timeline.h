#pragma once

#include "tools/timeline/timeline_accessor.h"

class MockCurrentFrameReader : public ICurrentFrameReader {
    int frame_ = 0;
public:
    void set(int f) { frame_ = f; }
    int getFrame() const override { return frame_; }
};

class MockCurrentFrameWriter : public ICurrentFrameWriter {
    int frame_ = -1;
public:
    int written() const { return frame_; }
    void setFrame(int frame) override { frame_ = frame; }
};
