#include "XVideoView.h"

#include "XSDL.h"

extern "C" {
#include <libavcodec/avcodec.h>
}

#include <mutex>

class XVideoView::PImpl
{
public:
    PImpl(XVideoView *owenr);
    ~PImpl() = default;

public:
    XVideoView *owenr_   = nullptr;
    int         width_   = 0; ///< 材质宽高
    int         height_  = 0;
    int         scale_w_ = 0; ///< 显示大小
    int         scale_h_ = 0;
    Format      fmt_     = RGBA; ///< 像素格式
    std::mutex  mtx_;            ///< 确保线程安全
};

XVideoView::PImpl::PImpl(XVideoView *owenr) : owenr_(owenr)
{
}

XVideoView::XVideoView() : impl_(std::make_unique<XVideoView::PImpl>(this))
{
}

XVideoView::~XVideoView()
{
}

auto XVideoView::create(RenderType type) -> XVideoView *
{
    switch (type)
    {
        case XVideoView::SDL:
            return new XSDL;
            break;
        default:
            break;
    }
    return nullptr;
}

auto XVideoView::drawFrame(AVFrame *frame) -> bool
{
    if (!frame || !frame->data[0])
    {
        return false;
    }

    switch (frame->format)
    {
        case AV_PIX_FMT_YUV420P:
            return draw(frame->data[0], frame->linesize[0], /// Y
                        frame->data[1], frame->linesize[1], /// U
                        frame->data[2], frame->linesize[2]  /// V
            );
        case AV_PIX_FMT_BGRA:
            return draw(frame->data[0], frame->linesize[0]);
        default:
            break;
    }

    return false;
}

auto XVideoView::scale(int w, int h) -> void
{
    impl_->scale_w_ = w;
    impl_->scale_h_ = h;
}

auto XVideoView::setScaleWidth(int w) -> void
{
    impl_->scale_w_ = w;
}

auto XVideoView::setScaleHeight(int h) -> void
{
    impl_->scale_h_ = h;
}

auto XVideoView::scaleWidth() -> int
{
    return impl_->scale_w_;
}

auto XVideoView::scaleHeight() -> int
{
    return impl_->scale_h_;
}
