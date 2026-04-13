#include "XSDL.h"
#include "AVLog.h"
#include <SDL.h>
#include <mutex>
#include <thread>
#include <atomic>

class XSDL::PImpl
{
public:
    PImpl(XSDL *owner);
    ~PImpl() = default;

    auto initVideo() -> bool;
    auto createRendererAndTexture(int w, int h, unsigned int sdl_fmt) -> bool;
    auto destroyRendererAndTexture() -> void;
    auto ensureWindow(int w, int h, unsigned int sdl_fmt) -> bool;

public:
    XSDL                *owner_ = nullptr;
    std::atomic_bool     isInitFinished_{ false };
    std::recursive_mutex mtx_;
    SDL_Window          *win_     = nullptr;
    SDL_Renderer        *render_  = nullptr;
    SDL_Texture         *texture_ = nullptr;
    int                  width_   = 0;
    int                  height_  = 0;
    unsigned int         format_  = SDL_PIXELFORMAT_RGBA32;
    std::thread::id      main_thread_id_;
    std::atomic_bool     is_destroying_{ false };
    std::atomic_bool     window_created_{ false };
    OverlayCallback      overlay_cb_ = nullptr;
};

XSDL::PImpl::PImpl(XSDL *owner) : owner_(owner)
{
    main_thread_id_ = std::this_thread::get_id();
}

auto XSDL::PImpl::initVideo() -> bool
{
    std::unique_lock<std::recursive_mutex> sdl_lock(mtx_);
    if (!isInitFinished_)
    {
        if (SDL_Init(SDL_INIT_VIDEO))
        {
            LOGE("SDL_Init failed: " << SDL_GetError());
            return false;
        }
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
        isInitFinished_ = true;
        LOGI("SDL初始化成功");
        return true;
    }
    return false;
}

auto XSDL::PImpl::destroyRendererAndTexture() -> void
{
    if (is_destroying_)
    {
        return;
    }

    std::unique_lock<std::recursive_mutex> lock(mtx_);

    if (texture_)
    {
        SDL_DestroyTexture(texture_);
        texture_ = nullptr;
        LOGD("SDL纹理已销毁");
    }
    if (render_)
    {
        SDL_DestroyRenderer(render_);
        render_ = nullptr;
        LOGD("SDL渲染器已销毁");
    }
}

auto XSDL::PImpl::createRendererAndTexture(int w, int h, unsigned int sdl_fmt) -> bool
{
    std::unique_lock<std::recursive_mutex> lock(mtx_);

    render_ = SDL_CreateRenderer(win_, -1, SDL_RENDERER_ACCELERATED);
    if (!render_)
    {
        LOGE("SDL_CreateRenderer failed: " << SDL_GetError());
        return false;
    }

    texture_ = SDL_CreateTexture(render_, sdl_fmt, SDL_TEXTUREACCESS_STREAMING, w, h);
    if (!texture_)
    {
        LOGE("SDL_CreateTexture failed: " << SDL_GetError());
        SDL_DestroyRenderer(render_);
        render_ = nullptr;
        return false;
    }

    width_  = w;
    height_ = h;
    format_ = sdl_fmt;
    LOGD("SDL渲染器和纹理创建成功");
    return true;
}

auto XSDL::PImpl::ensureWindow(int w, int h, unsigned int sdl_fmt) -> bool
{
    std::unique_lock<std::recursive_mutex> lock(mtx_);

    if (win_)
    {
        destroyRendererAndTexture();
        return createRendererAndTexture(w, h, sdl_fmt);
    }

    Uint32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;

    if (!owner_->hasWin())
    {
        win_ = SDL_CreateWindow("", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, w, h, flags);
    }
    else
    {
        win_ = SDL_CreateWindowFrom(owner_->window());
    }

    if (!win_)
    {
        LOGE("SDL_CreateWindow failed: " << SDL_GetError());
        return false;
    }

    LOGI("SDL窗口创建成功（永久存活）");
    window_created_ = true;

    return createRendererAndTexture(w, h, sdl_fmt);
}

XSDL::XSDL() : impl_(std::make_unique<XSDL::PImpl>(this))
{
    LOGD("XSDL创建");
}

XSDL::~XSDL()
{
    LOGD("XSDL销毁");
}

void XSDL::setOverlayCallback(OverlayCallback cb)
{
    impl_->overlay_cb_ = std::move(cb);
}

auto XSDL::resetRenderer() -> void
{
    LOGI("重置渲染器，保留窗口");
    impl_->destroyRendererAndTexture();
}

auto XSDL::init(int w, int h, Format fmt) -> bool
{
    if (w <= 0 || h <= 0)
    {
        return false;
    }

    impl_->initVideo();

    setWidth(w);
    setHeight(h);
    setFormat(fmt);

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

    return impl_->ensureWindow(w, h, sdl_fmt);
}

auto XSDL::isExit() -> bool
{
    SDL_Event ev;
    SDL_WaitEventTimeout(&ev, 1);
    return ev.type == SDL_QUIT;
}

auto XSDL::close() -> void
{
    LOGI("XSDL::close 被调用，但窗口永久存活，只重置渲染器");
    if (impl_->texture_)
    {
        SDL_DestroyTexture(impl_->texture_);
        impl_->texture_ = nullptr;
        LOGD("SDL纹理已销毁");
    }
    if (impl_->render_)
    {
        SDL_DestroyRenderer(impl_->render_);
        impl_->render_ = nullptr;
        LOGD("SDL渲染器已销毁");
    }

    if (impl_->win_)
    {
        SDL_DestroyWindow(impl_->win_);
        impl_->win_ = nullptr;
        LOGD("SDL窗口已销毁");
    }
}

auto XSDL::draw(const unsigned char *data, int lineSize) -> bool
{
    if (!data)
    {
        return false;
    }

    std::unique_lock<std::recursive_mutex> sdl_lock(impl_->mtx_);
    if (!impl_->texture_ || !impl_->render_ || !impl_->win_)
    {
        return false;
    }

    int w = width();
    int h = height();
    if (w <= 0 || h <= 0)
    {
        return false;
    }

    if (lineSize <= 0)
    {
        switch (format())
        {
            case XVideoView::BGRA:
            case XVideoView::RGBA:
            case XVideoView::ARGB:
                lineSize = w * 4;
                break;
            case XVideoView::YUV420P:
                lineSize = w;
                break;
            default:
                break;
        }
    }

    auto ret = SDL_UpdateTexture(impl_->texture_, NULL, data, lineSize);
    if (ret != 0)
    {
        LOGE("SDL_UpdateTexture failed: " << SDL_GetError());
        return false;
    }

    SDL_RenderClear(impl_->render_);

    if (scaleWidth() <= 0)
    {
        setScaleWidth(w);
    }

    if (scaleHeight() <= 0)
    {
        setScaleHeight(h);
    }

    SDL_Rect rect = { 0, 0, scaleWidth(), scaleHeight() };
    ret           = SDL_RenderCopy(impl_->render_, impl_->texture_, NULL, &rect);
    if (ret != 0)
    {
        LOGE("SDL_RenderCopy failed: " << SDL_GetError());
        return false;
    }

    if (impl_->overlay_cb_)
    {
        impl_->overlay_cb_(impl_->render_);
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

    std::unique_lock<std::recursive_mutex> sdl_lock(impl_->mtx_);
    if (!impl_->texture_ || !impl_->render_ || !impl_->win_)
    {
        return false;
    }

    int w = width();
    int h = height();
    if (w <= 0 || h <= 0)
    {
        return false;
    }

    auto ret = SDL_UpdateYUVTexture(impl_->texture_, NULL, y, y_pitch, u, u_pitch, v, v_pitch);
    if (ret != 0)
    {
        LOGE("SDL_UpdateYUVTexture failed: " << SDL_GetError());
        return false;
    }

    SDL_RenderClear(impl_->render_);

    if (scaleWidth() <= 0)
    {
        setScaleWidth(w);
    }

    if (scaleHeight() <= 0)
    {
        setScaleHeight(h);
    }

    SDL_Rect rect = { 0, 0, scaleWidth(), scaleHeight() };
    ret           = SDL_RenderCopy(impl_->render_, impl_->texture_, NULL, &rect);
    if (ret != 0)
    {
        LOGE("SDL_RenderCopy failed: " << SDL_GetError());
        return false;
    }

    if (impl_->overlay_cb_)
    {
        impl_->overlay_cb_(impl_->render_);
    }

    SDL_RenderPresent(impl_->render_);
    return true;
}

void *XSDL::getSDLRenderer()
{
    return impl_->render_;
}
