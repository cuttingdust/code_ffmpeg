#include "FrameGenerator.h"

extern "C" {
#include <libavutil/frame.h>
}

class FrameGenerator::PImpl
{
public:
    PImpl(FrameGenerator *owenr, int w, int h, AVPixelFormat fmt);
    ~PImpl() = default;

public:
    FrameGenerator *owenr_ = nullptr;
    int             width_;
    int             height_;
    AVPixelFormat   pix_fmt_;
};

FrameGenerator::PImpl::PImpl(FrameGenerator *owenr, int w, int h, AVPixelFormat fmt) :
    owenr_(owenr), width_(w), height_(h), pix_fmt_(fmt)
{
}

FrameGenerator::FrameGenerator(int w, int h, AVPixelFormat fmt) : impl_(std::make_unique<PImpl>(this, w, h, fmt))
{
}

FrameGenerator::~FrameGenerator() = default;


auto FrameGenerator::generate_frame(int frame_index) -> FrameWrapper
{
    FrameWrapper frame;
    frame->width  = impl_->width_;
    frame->height = impl_->height_;
    frame->format = impl_->pix_fmt_;
    frame->pts    = frame_index;

    frame.allocate_buffer();

    /// Y平面
    for (int y = 0; y < impl_->height_; ++y)
    {
        for (int x = 0; x < impl_->width_; ++x)
        {
            frame->data[0][y * frame->linesize[0] + x] = (x + y + frame_index * 3) & 0xFF;
        }
    }

    /// U平面
    for (int y = 0; y < impl_->height_ / 2; ++y)
    {
        for (int x = 0; x < impl_->width_ / 2; ++x)
        {
            frame->data[1][y * frame->linesize[1] + x] = (128 + y + frame_index * 2) & 0xFF;
        }
    }

    /// V平面
    for (int y = 0; y < impl_->height_ / 2; y++)
    {
        for (int x = 0; x < impl_->width_ / 2; x++)
        {
            frame->data[2][y * frame->linesize[2] + x] = (64 + x + frame_index * 5) & 0xFF;
        }
    }

    /// 设置帧类型提示
    if (frame_index % 12 == 0)
    {
        frame->pict_type = AV_PICTURE_TYPE_I;
    }
    else
    {
        frame->pict_type = AV_PICTURE_TYPE_NONE;
    }

    return frame;
}
