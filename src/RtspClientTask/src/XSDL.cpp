#include "XSDL.h"

#include <iostream>
#include <SDL.h>

#include <mutex>


class XSDL::PImpl
{
public:
    PImpl(XSDL *owenr);
    ~PImpl() = default;

public:
    auto initVideo() -> bool;

public:
    XSDL            *owenr_ = nullptr;
    std::atomic_bool isInitFinished_{ false };
    std::mutex       mtx_;
    SDL_Window      *win_     = nullptr;
    SDL_Renderer    *render_  = nullptr;
    SDL_Texture     *texture_ = nullptr;
};

XSDL::PImpl::PImpl(XSDL *owenr) : owenr_(owenr)
{
}

auto XSDL::PImpl::initVideo() -> bool
{
    std::unique_lock<std::mutex> sdl_lock(mtx_);
    if (!isInitFinished_)
    {
        if (SDL_Init(SDL_INIT_VIDEO))
        {
            std::cout << SDL_GetError() << std::endl;
            return false;
        }
        /// 设定缩放算法，解决锯齿问题,线性插值算法
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");

        isInitFinished_ = true;
        return true;
    }

    return false;
}

XSDL::XSDL() : impl_(std::make_unique<XSDL::PImpl>(this))
{
}

XSDL::~XSDL()
{
}

auto XSDL::init(int w, int h, Format fmt) -> bool
{
    if (w <= 0 || h <= 0)
    {
        return false;
    }

    /// 初始化SDL 视频库
    impl_->initVideo();

    /// 确保线程安全
    std::unique_lock<std::mutex> sdl_lock(impl_->mtx_);
    this->setWidth(w);
    this->setHeight(h);
    this->setFormat(fmt);

    if (impl_->texture_)
    {
        SDL_DestroyTexture(impl_->texture_);
    }

    if (impl_->render_)
    {
        SDL_DestroyRenderer(impl_->render_);
    }


    ///1 创建窗口
    if (!impl_->win_)
    {
        if (!hasWin())
        {
            /// 新建窗口
            impl_->win_ = SDL_CreateWindow("", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, w, h,
                                           SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
        }
        else
        {
            impl_->win_ = SDL_CreateWindowFrom(window()); /// 渲染到控件窗口
        }
    }

    if (!impl_->win_)
    {
        std::cerr << SDL_GetError() << std::endl;
        return false;
    }

    /// 2 创建渲染器
    impl_->render_ = SDL_CreateRenderer(impl_->win_, -1, SDL_RENDERER_ACCELERATED); /// 硬件加速
    if (!impl_->render_)
    {
        std::cerr << SDL_GetError() << std::endl;
        return false;
    }

    /// 创建材质 （显存）
    unsigned int sdl_fmt = SDL_PIXELFORMAT_RGBA8888;
    switch (fmt)
    {
        case XVideoView::RGBA:
            sdl_fmt = SDL_PIXELFORMAT_RGBA32;
            break;
        case XVideoView::BGRA:
            sdl_fmt = SDL_PIXELFORMAT_BGRA32;
            break;
        case XVideoView::ARGB:
            sdl_fmt = SDL_PIXELFORMAT_ARGB32;
            break;
        case XVideoView::YUV420P:
            sdl_fmt = SDL_PIXELFORMAT_IYUV;
            break;
        case XVideoView::NV12:
            sdl_fmt = SDL_PIXELFORMAT_NV12;
            break;
        default:
            break;
    }

    impl_->texture_ = SDL_CreateTexture(impl_->render_,
                                        sdl_fmt,                     /// 像素格式
                                        SDL_TEXTUREACCESS_STREAMING, /// 频繁修改的渲染（带锁）
                                        w, h                         /// 材质大小
    );

    if (!impl_->texture_)
    {
        std::cerr << SDL_GetError() << std::endl;
        return false;
    }

    return true;
}

auto XSDL::isExit() -> bool
{
    SDL_Event ev;
    SDL_WaitEventTimeout(&ev, 1);
    if (ev.type == SDL_QUIT)
    {
        return true;
    }

    return false;
}

auto XSDL::close() -> void
{
    /// 确保线程安全
    std::unique_lock<std::mutex> sdl_lock(impl_->mtx_);
    if (impl_->texture_)
    {
        SDL_DestroyTexture(impl_->texture_);
        impl_->texture_ = nullptr;
    }

    if (impl_->render_)
    {
        SDL_DestroyRenderer(impl_->render_);
        impl_->render_ = nullptr;
    }

    if (impl_->win_)
    {
        SDL_DestroyWindow(impl_->win_);
        impl_->win_ = nullptr;
    }
}

auto XSDL::draw(const unsigned char *data, int lineSize) -> bool
{
    if (!data)
    {
        return false;
    }

    std::unique_lock<std::mutex> sdl_lock(impl_->mtx_);

    if (!impl_->texture_ || !impl_->render_ || !impl_->win_)
    {
        return false;
    }

    if (width() <= 0 || height() <= 0)
    {
        return false;
    }

    if (lineSize <= 0)
    {
        switch (auto fmt = format())
        {
            case XVideoView::BGRA:
            case XVideoView::RGBA:
            case XVideoView::ARGB:
                {
                    lineSize = width() * 4;
                    break;
                }
            case XVideoView::YUV420P:
                {
                    lineSize = width();
                    break;
                }
            default:
                break;
        }
    }

    if (lineSize <= 0)
    {
        return false;
    }

    /// 复制内存到显显存
    auto ret = SDL_UpdateTexture(impl_->texture_, NULL, data, lineSize);
    if (ret != 0)
    {
        std::cout << SDL_GetError() << std::endl;
        return false;
    }
    /// 清空屏幕
    SDL_RenderClear(impl_->render_);

    /// 材质复制到渲染器
    if (scaleWidth() <= 0)
    {
        setScaleWidth(width());
    }
    if (scaleHeight() <= 0)
    {
        setScaleHeight(height());
    }

    SDL_Rect rect;
    rect.x = 0;
    rect.y = 0;
    rect.w = scaleWidth(); /// 渲染的宽高，可缩放
    rect.h = scaleHeight();
    ret    = SDL_RenderCopy(impl_->render_, impl_->texture_, NULL, &rect);
    if (ret != 0)
    {
        std::cout << SDL_GetError() << std::endl;
        return false;
    }
    SDL_RenderPresent(impl_->render_);
    return true;
}

auto XSDL::draw(const unsigned char *y, int y_pitch, const unsigned char *u, int u_pitch, const unsigned char *v,
                int v_pitch) -> bool
{
    if (!y || !u || !v)
    {
        return false;
    }
    std::unique_lock<std::mutex> sdl_lock(impl_->mtx_);
    if (!impl_->texture_ || !impl_->render_ || !impl_->win_)
    {
        return false;
    }
    if (width() <= 0 || height() <= 0)
    {
        return false;
    }

    /// 复制内存到显显存
    auto ret = SDL_UpdateYUVTexture(impl_->texture_, NULL, y, y_pitch, u, u_pitch, v, v_pitch);
    if (ret != 0)
    {
        std::cout << SDL_GetError() << std::endl;
        return false;
    }
    /// 清空屏幕
    SDL_RenderClear(impl_->render_);
    /// 材质复制到渲染器
    if (scaleWidth() <= 0)
    {
        setScaleWidth(width());
    }
    if (scaleHeight() <= 0)
    {
        setScaleHeight(height());
    }
    SDL_Rect rect;
    rect.x = 0;
    rect.y = 0;
    rect.w = scaleWidth(); /// 渲染的宽高，可缩放
    rect.h = scaleHeight();
    ret    = SDL_RenderCopy(impl_->render_, impl_->texture_, NULL, &rect);
    if (ret != 0)
    {
        std::cout << SDL_GetError() << std::endl;
        return false;
    }
    SDL_RenderPresent(impl_->render_);
    return true;
}
