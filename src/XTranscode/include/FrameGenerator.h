#pragma once
#include "AVConst.h"
#include "FrameWrapper.h"

/// ==================== 测试帧生成器 ====================
class FrameGenerator
{
public:
    FrameGenerator(int w, int h, AVPixelFormat fmt = AV_PIX_FMT_YUV420P);
    ~FrameGenerator();

public:
    auto generate_frame(int frame_index) -> FrameWrapper;

private:
    class PImpl;
    std::unique_ptr<PImpl> impl_;
};
