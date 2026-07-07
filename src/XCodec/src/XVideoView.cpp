#include "XVideoView.h"
#include "AVConst.h"
#include "XSDL.h"
#include <mutex>
#include <fstream>

/**
 * @brief XVideoView 的私有实现类（PImpl模式）
 * 
 * 使用PImpl模式隐藏实现细节，避免暴露内部数据结构
 * 同时减少编译依赖，提高封装性
 */
class XVideoView::PImpl
{
public:
    /**
     * @brief 构造函数
     * @param owner 拥有者XVideoView指针
     */
    PImpl(XVideoView *owner);
    ~PImpl();

public:
    XVideoView *owner_   = nullptr; ///< 拥有者指针
    void       *win_id_  = nullptr; ///< 外部窗口句柄（如果使用外部窗口）
    int         width_   = 0;       ///< 视频原始宽度
    int         height_  = 0;       ///< 视频原始高度
    int         scale_w_ = 0;       ///< 显示缩放宽度
    int         scale_h_ = 0;       ///< 显示缩放高度
    Format      fmt_     = RGBA;    ///< 当前像素格式
    std::mutex  mtx_;               ///< 线程安全互斥锁

    // FPS统计相关
    int       render_fps_ = 0; ///< 当前渲染帧率
    long long beg_ms_     = 0; ///< 开始计时的时间戳（毫秒）
    int       count_      = 0; ///< 统计周期内的帧计数

    // 文件读取相关
    std::ifstream ifs_;             ///< 输入文件流
    AVFrame      *frame_ = nullptr; ///< 当前帧（用于文件读取模式）

    // NV12格式转换缓存
    unsigned char *cache_ = nullptr; ///< NV12转RGB的临时缓冲区
};

XVideoView::PImpl::PImpl(XVideoView *owner) : owner_(owner)
{
}

XVideoView::PImpl::~PImpl()
{
    delete[] cache_;
    cache_ = nullptr;
    if (frame_)
    {
        av_frame_free(&frame_);
        frame_ = nullptr;
    }
}

XVideoView::XVideoView() : impl_(std::make_unique<XVideoView::PImpl>(this))
{
}

XVideoView::~XVideoView() = default;

/**
 * @brief 创建视频渲染器实例
 * @param type 渲染后端类型
 * @return 渲染器指针
 */
auto XVideoView::create(RenderType type) -> XVideoView *
{
    switch (type)
    {
        case XVideoView::SDL:
            return new XSDL;
        default:
            break;
    }
    return nullptr;
}

void XVideoView::setOverlayCallback(OverlayCallback cb)
{
}

/**
 * @brief 渲染AVFrame格式的帧
 * @param frame FFmpeg AVFrame指针
 * @return 成功返回true，失败返回false
 * 
 * 支持多种格式：
 * - YUV420P: 直接调用三平面渲染
 * - NV12: 转换为连续内存后渲染
 * - BGRA/ARGB/RGBA: 直接作为单平面渲染
 */
auto XVideoView::drawFrame(AVFrame *frame) -> bool
{
    if (!frame || !frame->data[0])
    {
        return false;
    }

    /// 更新FPS计数器
    impl_->count_++;

    if (impl_->beg_ms_ <= 0)
    {
        impl_->beg_ms_ = clock();
    }
    /// 每秒计算一次FPS
    else if ((clock() - impl_->beg_ms_) / (CLOCKS_PER_SEC / 1000) >= 1000)
    {
        impl_->render_fps_ = impl_->count_;
        impl_->count_      = 0;
        impl_->beg_ms_     = clock();
    }

    int linesize = 0;
    switch (frame->format)
    {
        case AV_PIX_FMT_YUV420P:
            /// YUV420P: 三个平面分别渲染
            return draw(frame->data[0], frame->linesize[0], /// Y平面
                        frame->data[1], frame->linesize[1], /// U平面
                        frame->data[2], frame->linesize[2]  /// V平面
            );

        case AV_PIX_FMT_NV12:
            /// NV12: Y平面 + UV交错平面，需要合并到连续内存
            if (!impl_->cache_)
            {
                /// 分配足够的缓存空间 (Y + UV)
                impl_->cache_ = new unsigned char[4096 * 2160 * 1.5];
            }
            linesize = frame->width;

            if (frame->linesize[0] == frame->width)
            {
                /// 如果行宽等于宽度，可以直接整块拷贝
                memcpy(impl_->cache_, frame->data[0], frame->linesize[0] * frame->height); /// Y平面
                memcpy(impl_->cache_ + frame->linesize[0] * frame->height, frame->data[1],
                       frame->linesize[1] * frame->height / 2); /// UV平面
            }
            else
            {
                /// 否则需要逐行拷贝（处理行对齐问题）
                for (int i = 0; i < frame->height; i++) /// Y平面逐行拷贝
                {
                    memcpy(impl_->cache_ + i * frame->width, frame->data[0] + i * frame->linesize[0], frame->width);
                }
                for (int i = 0; i < frame->height / 2; i++) /// UV平面逐行拷贝
                {
                    auto p = impl_->cache_ + frame->height * frame->width; /// 跳过Y平面
                    memcpy(p + i * frame->width, frame->data[1] + i * frame->linesize[1], frame->width);
                }
            }

            return draw(impl_->cache_, linesize);

        case AV_PIX_FMT_BGRA:
        case AV_PIX_FMT_ARGB:
        case AV_PIX_FMT_RGBA:
            /// 32位RGB格式，直接作为单平面渲染
            return draw(frame->data[0], frame->linesize[0]);

        default:
            break;
    }

    return false;
}

/**
 * @brief 设置显示缩放大小
 * @param w 显示宽度
 * @param h 显示高度
 */
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

/**
 * @brief 打开视频文件（用于直接读取帧）
 * @param filepath 文件路径
 * @return 成功返回true
 * @note 文件格式需要与设置的宽度、高度、像素格式匹配
 */
auto XVideoView::open(const std::string &filepath) -> bool
{
    if (impl_->ifs_.is_open())
    {
        impl_->ifs_.close();
    }
    impl_->ifs_.open(filepath, std::ios::binary);
    return impl_->ifs_.is_open();
}

/**
 * @brief 从打开的文件中读取一帧数据
 * @return AVFrame指针，失败返回nullptr
 * @note 根据当前设置的宽度、高度、像素格式读取相应大小的数据
 */
auto XVideoView::read() -> AVFrame *
{
    if (impl_->width_ <= 0 || impl_->height_ <= 0 || !impl_->ifs_)
    {
        return nullptr;
    }

    /// 如果参数发生变化，需要重新分配AVFrame
    if (impl_->frame_)
    {
        if (impl_->frame_->width != impl_->width_ || impl_->frame_->height != impl_->height_ ||
            impl_->frame_->format != impl_->fmt_)
        {
            av_frame_free(&impl_->frame_);
        }
    }

    /// 分配AVFrame空间
    if (!impl_->frame_)
    {
        impl_->frame_         = av_frame_alloc();
        impl_->frame_->width  = impl_->width_;
        impl_->frame_->height = impl_->height_;
        impl_->frame_->format = impl_->fmt_;

        /// 根据格式设置行宽
        if (impl_->frame_->format == AV_PIX_FMT_YUV420P)
        {
            impl_->frame_->linesize[0] = impl_->width_;     /// Y平面
            impl_->frame_->linesize[1] = impl_->width_ / 2; /// U平面
            impl_->frame_->linesize[2] = impl_->width_ / 2; /// V平面
        }
        else
        {
            impl_->frame_->linesize[0] = impl_->width_ * 4; /// 32位RGB
        }

        /// 分配帧数据缓冲区
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
                         impl_->frame_->linesize[0] * impl_->height_); /// Y平面
        impl_->ifs_.read((char *)impl_->frame_->data[1],
                         impl_->frame_->linesize[1] * (impl_->height_ / 2)); /// U平面
        impl_->ifs_.read((char *)impl_->frame_->data[2],
                         impl_->frame_->linesize[2] * (impl_->height_ / 2)); /// V平面
    }
    else /// RGBA/ARGB/BGRA 32位格式
    {
        impl_->ifs_.read((char *)impl_->frame_->data[0], impl_->frame_->linesize[0] * impl_->height_);
    }

    /// 检查是否读取到数据
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
