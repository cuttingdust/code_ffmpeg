#include "XVideoView.h"

#include "XSDL.h"

#include <iostream>

extern "C" {
#include <libavcodec/avcodec.h>
}

#include <mutex>
#include <fstream>

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

long long NowMs()
{
    return clock() / (CLOCKS_PER_SEC / 1000);
}

class XVideoView::PImpl
{
public:
    PImpl(XVideoView *owenr);
    ~PImpl();

public:
    XVideoView *owenr_   = nullptr;
    void       *win_id_  = nullptr; ///< 窗口句柄
    int         width_   = 0;       ///< 材质宽高
    int         height_  = 0;
    int         scale_w_ = 0; ///< 显示大小
    int         scale_h_ = 0;
    Format      fmt_     = RGBA; ///< 像素格式
    std::mutex  mtx_;            ///< 确保线程安全

    int       render_fps_ = 0; ///< 显示帧率
    long long beg_ms_     = 0; ///< 计时开始时间
    int       count_      = 0; ///< 统计显示次数

    std::ifstream ifs_;
    AVFrame      *frame_ = nullptr;

    unsigned char *cache_ = nullptr; /// 复制NV12缓冲
};

XVideoView::PImpl::PImpl(XVideoView *owenr) : owenr_(owenr)
{
}

XVideoView::PImpl ::~PImpl()
{
    delete cache_;
    cache_ = nullptr;
}

XVideoView::XVideoView() : impl_(std::make_unique<XVideoView::PImpl>(this))
{
}

XVideoView::~XVideoView() = default;

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

    int linesize = 0;
    switch (frame->format)
    {
        case AV_PIX_FMT_YUV420P:
            return draw(frame->data[0], frame->linesize[0], /// Y
                        frame->data[1], frame->linesize[1], /// U
                        frame->data[2], frame->linesize[2]  /// V
            );
        case AV_PIX_FMT_NV12:
            if (!impl_->cache_)
            {
                impl_->cache_ = new unsigned char[4096 * 2160 * 1.5];
            }
            linesize = frame->width;
            if (frame->linesize[0] == frame->width)
            {
                memcpy(impl_->cache_, frame->data[0], frame->linesize[0] * frame->height); /// Y
                memcpy(impl_->cache_ + frame->linesize[0] * frame->height, frame->data[1],
                       frame->linesize[1] * frame->height / 2); /// UV
            }
            else /// 逐行复制
            {
                for (int i = 0; i < frame->height; i++) /// Y
                {
                    memcpy(impl_->cache_ + i * frame->width, frame->data[0] + i * frame->linesize[0], frame->width);
                }
                for (int i = 0; i < frame->height / 2; i++) /// UV
                {
                    auto p = impl_->cache_ + frame->height * frame->width; /// 移位Y
                    memcpy(p + i * frame->width, frame->data[1] + i * frame->linesize[1], frame->width);
                }
            }

            //frame->data[0] + frame->data[1]
            return draw(impl_->cache_, linesize);
        case AV_PIX_FMT_BGRA:
        case AV_PIX_FMT_ARGB:
        case AV_PIX_FMT_RGBA:
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

auto XVideoView::open(const std::string &filepath) -> bool
{
    if (impl_->ifs_.is_open())
    {
        impl_->ifs_.close();
    }
    impl_->ifs_.open(filepath, std::ios::binary);
    return impl_->ifs_.is_open();
}

auto XVideoView::read() -> AVFrame *
{
    if (impl_->width_ <= 0 || impl_->height_ <= 0 || !impl_->ifs_)
    {
        return nullptr;
    }

    /// AVFrame空间已经申请，如果参数发生变化，需要释放空间
    if (impl_->frame_)
    {
        if (impl_->frame_->width != impl_->width_ || impl_->frame_->height != impl_->height_ ||
            impl_->frame_->format != impl_->fmt_)
        {
            /// 释放AVFrame对象空间，和buf引用计数减一
            av_frame_free(&impl_->frame_);
        }
    }

    if (!impl_->frame_)
    {
        /// 分配对象空间和像素空间
        impl_->frame_         = av_frame_alloc();
        impl_->frame_->width  = impl_->width_;
        impl_->frame_->height = impl_->height_;
        impl_->frame_->format = impl_->fmt_;

        if (impl_->frame_->format == AV_PIX_FMT_YUV420P)
        {
            impl_->frame_->linesize[0] = impl_->width_;     /// Y
            impl_->frame_->linesize[1] = impl_->width_ / 2; /// U
            impl_->frame_->linesize[2] = impl_->width_ / 2; /// V
        }
        else
        {
            impl_->frame_->linesize[0] = impl_->width_ * 4;
        }
        /// 生成AVFrame空间，使用默认对齐
        auto re = av_frame_get_buffer(impl_->frame_, 0);
        if (re != 0)
        {
            char buf[1024] = { 0 };
            av_strerror(re, buf, sizeof(buf) - 1);
            std::cout << buf << std::endl;
            av_frame_free(&impl_->frame_);
            return nullptr;
        }
    }

    if (!impl_->frame_)
    {
        return nullptr;
    }


    /// 读取一帧数据
    if (impl_->frame_->format == AV_PIX_FMT_YUV420P)
    {
        impl_->ifs_.read((char *)impl_->frame_->data[0],
                         impl_->frame_->linesize[0] * impl_->height_); /// Y
        impl_->ifs_.read((char *)impl_->frame_->data[1],
                         impl_->frame_->linesize[1] * (impl_->height_ / 2)); /// U
        impl_->ifs_.read((char *)impl_->frame_->data[2],
                         impl_->frame_->linesize[2] * (impl_->height_ / 2)); /// V
    }
    else /// RGBA ARGB BGRA 32
    {
        impl_->ifs_.read((char *)impl_->frame_->data[0], impl_->frame_->linesize[0] * impl_->height_);
    }


    if (impl_->ifs_.gcount() == 0)
        return nullptr;

    return impl_->frame_;
}

auto XVideoView::setWindow(void *win) -> void
{
    impl_->win_id_ = win;
}

auto XVideoView::window() const -> void *
{
    return impl_->win_id_;
}

auto XVideoView::hasWin() -> bool
{
    return impl_->win_id_ != nullptr;
}

auto XVideoView::setWidth(int width) -> void
{
    impl_->width_ = width;
}

auto XVideoView::width() const -> int
{
    return impl_->width_;
}

auto XVideoView::setHeight(int height) -> void
{
    impl_->height_ = height;
}

auto XVideoView::height() const -> int
{
    return impl_->height_;
}

auto XVideoView::setFormat(const Format &fmt) -> void
{
    impl_->fmt_ = fmt;
}

auto XVideoView::format() const -> Format
{
    return impl_->fmt_;
}
