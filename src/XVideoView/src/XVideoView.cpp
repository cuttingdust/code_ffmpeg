#include "XVideoView.h"

#include "XSDL.h"

extern "C" {
#include <libavcodec/avcodec.h>
}

#include <mutex>
#include <utility>

void MSleep(unsigned int ms)
{
    const auto beg = clock();
    for (int i = 0; std::cmp_less(i, ms); i++)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        if (std::cmp_greater_equal((clock() - beg) / (CLOCKS_PER_SEC / 1000), ms))
        {
            break;
        }
    }
}

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

    int       render_fps_ = 0; ///< 显示帧率
    long long beg_ms_     = 0; ///< 计时开始时间
    int       count_      = 0; ///< 统计显示次数
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
    impl_->count_++;

    if (impl_->beg_ms_ <= 0)
    {
        impl_->beg_ms_ = clock();
    }
    /// 计算显示帧率
    else if ((clock() - impl_->beg_ms_) / (CLOCKS_PER_SEC / 1000) >= 1000) /// 一秒计算一次fps
    {
        impl_->render_fps_ = impl_->count_;
        impl_->count_      = 0;
        impl_->beg_ms_     = clock();
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

auto XVideoView::renderFps() const -> int
{
    return impl_->render_fps_;
}
