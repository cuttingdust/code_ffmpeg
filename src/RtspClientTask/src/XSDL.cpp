#include "XSDL.h"
#include "AVLog.h"
#include <SDL.h>
#include <mutex>
#include <thread>
#include <atomic>

/**
 * @brief XSDL的私有实现类（PImpl模式）
 * 
 * 封装所有SDL相关的资源和操作，实现：
 * - 线程安全的资源访问
 * - 窗口的永久存活管理
 * - 渲染器和纹理的创建与销毁
 */
class XSDL::PImpl
{
public:
    PImpl(XSDL *owner);
    ~PImpl() = default;

public:
    /**
     * @brief 初始化SDL视频子系统
     * @return 成功返回true，失败返回false
     * @note 只会执行一次
     */
    auto initVideo() -> bool;

    /**
     * @brief 创建渲染器和纹理
     * @param w 纹理宽度
     * @param h 纹理高度
     * @param sdl_fmt SDL像素格式
     * @return 成功返回true，失败返回false
     */
    auto createRendererAndTexture(int w, int h, unsigned int sdl_fmt) -> bool;

    /**
     * @brief 销毁渲染器和纹理
     */
    auto destroyRendererAndTexture() -> void;

    /**
     * @brief 确保窗口存在
     * @param w 窗口宽度
     * @param h 窗口高度
     * @param sdl_fmt SDL像素格式
     * @return 成功返回true，失败返回false
     * @note 窗口不存在时创建，存在时只重置渲染器
     */
    auto ensureWindow(int w, int h, unsigned int sdl_fmt) -> bool;

public:
    XSDL                *owner_ = nullptr;                  ///< 拥有者指针
    std::atomic_bool     isInitFinished_{ false };          ///< SDL初始化完成标志
    std::recursive_mutex mtx_;                              ///< 递归互斥锁（避免死锁）
    SDL_Window          *win_     = nullptr;                ///< SDL窗口指针
    SDL_Renderer        *render_  = nullptr;                ///< SDL渲染器指针
    SDL_Texture         *texture_ = nullptr;                ///< SDL纹理指针
    int                  width_   = 0;                      ///< 纹理宽度
    int                  height_  = 0;                      ///< 纹理高度
    unsigned int         format_  = SDL_PIXELFORMAT_RGBA32; ///< SDL像素格式
    std::thread::id      main_thread_id_;                   ///< 主线程ID
    std::atomic_bool     is_destroying_{ false };           ///< 销毁中标志
    std::atomic_bool     window_created_{ false };          ///< 窗口已创建标志
};

/**
 * @brief 构造函数，记录主线程ID
 * @param owner 拥有者指针
 */
XSDL::PImpl::PImpl(XSDL *owner) : owner_(owner)
{
    main_thread_id_ = std::this_thread::get_id();
}

/**
 * @brief 初始化SDL视频子系统
 * @return 成功返回true，失败返回false
 * 
 * 设置渲染质量为线性插值，获得更好的缩放效果
 */
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
        /// 设置缩放算法为线性插值，避免锯齿
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
        isInitFinished_ = true;
        LOGI("SDL初始化成功");
        return true;
    }
    return false;
}

/**
 * @brief 销毁渲染器和纹理
 * 
 * 使用递归锁确保线程安全
 * 注意：不销毁窗口，实现窗口永久存活
 */
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

/**
 * @brief 创建渲染器和纹理
 * @param w 纹理宽度
 * @param h 纹理高度
 * @param sdl_fmt SDL像素格式
 * @return 成功返回true，失败返回false
 * 
 * 创建硬件加速的渲染器和流式纹理
 */
auto XSDL::PImpl::createRendererAndTexture(int w, int h, unsigned int sdl_fmt) -> bool
{
    std::unique_lock<std::recursive_mutex> lock(mtx_);

    /// 创建硬件加速渲染器
    render_ = SDL_CreateRenderer(win_, -1, SDL_RENDERER_ACCELERATED);
    if (!render_)
    {
        LOGE("SDL_CreateRenderer failed: " << SDL_GetError());
        return false;
    }

    /// 创建流式纹理（频繁更新）
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

/**
 * @brief 确保窗口存在
 * @param w 窗口宽度
 * @param h 窗口高度
 * @param sdl_fmt SDL像素格式
 * @return 成功返回true，失败返回false
 * 
 * 实现窗口永久存活策略：
 * - 第一次调用时创建窗口
 * - 后续调用只重置渲染器和纹理
 */
auto XSDL::PImpl::ensureWindow(int w, int h, unsigned int sdl_fmt) -> bool
{
    std::unique_lock<std::recursive_mutex> lock(mtx_);

    /// 窗口已存在，只重置渲染器和纹理
    if (win_)
    {
        destroyRendererAndTexture();
        return createRendererAndTexture(w, h, sdl_fmt);
    }

    /// 创建窗口（只在程序生命周期内创建一次）
    Uint32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;

    if (!owner_->hasWin())
    {
        /// 创建新窗口
        win_ = SDL_CreateWindow("", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, w, h, flags);
    }
    else
    {
        /// 嵌入外部窗口
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

// ==================== XSDL 公有接口实现 ====================

XSDL::XSDL() : impl_(std::make_unique<XSDL::PImpl>(this))
{
    LOGD("XSDL创建");
}

XSDL::~XSDL()
{
    LOGD("XSDL销毁");
    /// 注意：不销毁任何SDL资源，让操作系统在进程退出时自动清理
    /// 因为SDL资源可能在子线程被销毁，会导致崩溃
}

/**
 * @brief 重置渲染器
 * 
 * 在重连或流切换时调用，保留窗口只重置渲染器
 * 避免窗口闪烁和重建开销
 */
auto XSDL::resetRenderer() -> void
{
    LOGI("重置渲染器，保留窗口");
    impl_->destroyRendererAndTexture();
}

/**
 * @brief 初始化SDL渲染窗口
 * @param w 窗口宽度
 * @param h 窗口高度
 * @param fmt 像素格式
 * @return 成功返回true，失败返回false
 * 
 * 将XVideoView的像素格式转换为SDL像素格式
 * 然后创建或重置渲染器
 */
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

    /// 转换XVideoView格式到SDL格式
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

/**
 * @brief 检查窗口退出事件
 * @return 用户点击关闭窗口返回true
 * 
 * 使用超时等待事件，避免阻塞主线程
 */
auto XSDL::isExit() -> bool
{
    SDL_Event ev;
    SDL_WaitEventTimeout(&ev, 1);
    return ev.type == SDL_QUIT;
}

/**
 * @brief 关闭窗口
 * @note 由于窗口永久存活策略，只重置渲染器
 */
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

/**
 * @brief 渲染RGB/ARGB等单平面数据
 * @param data 图像数据指针
 * @param lineSize 一行数据的字节数
 * @return 成功返回true，失败返回false
 * 
 * 将CPU内存中的图像数据更新到GPU纹理
 * 然后复制到渲染器并显示
 */
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


    /// 自动计算行宽（如果未指定）
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

    /// 更新纹理数据
    auto ret = SDL_UpdateTexture(impl_->texture_, NULL, data, lineSize);
    if (ret != 0)
    {
        LOGE("SDL_UpdateTexture failed: " << SDL_GetError());
        return false;
    }

    /// 清空渲染器
    SDL_RenderClear(impl_->render_);

    /// 设置缩放大小
    if (scaleWidth() <= 0)
    {
        setScaleWidth(w);
    }

    if (scaleHeight() <= 0)
    {
        setScaleHeight(h);
    }


    /// 渲染纹理到窗口
    SDL_Rect rect = { 0, 0, scaleWidth(), scaleHeight() };
    ret           = SDL_RenderCopy(impl_->render_, impl_->texture_, NULL, &rect);
    if (ret != 0)
    {
        LOGE("SDL_RenderCopy failed: " << SDL_GetError());
        return false;
    }

    /// 显示渲染结果
    SDL_RenderPresent(impl_->render_);
    return true;
}

/**
 * @brief 渲染YUV420P格式数据
 * @param y Y平面数据指针
 * @param y_pitch Y平面一行字节数
 * @param u U平面数据指针
 * @param u_pitch U平面一行字节数
 * @param v V平面数据指针
 * @param v_pitch V平面一行字节数
 * @return 成功返回true，失败返回false
 * 
 * 使用SDL的专用YUV更新函数，效率更高
 */
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


    /// 使用SDL的YUV更新函数
    auto ret = SDL_UpdateYUVTexture(impl_->texture_, NULL, y, y_pitch, u, u_pitch, v, v_pitch);
    if (ret != 0)
    {
        LOGE("SDL_UpdateYUVTexture failed: " << SDL_GetError());
        return false;
    }

    /// 清空渲染器
    SDL_RenderClear(impl_->render_);

    /// 设置缩放大小
    if (scaleWidth() <= 0)
    {
        setScaleWidth(w);
    }

    if (scaleHeight() <= 0)
    {
        setScaleHeight(h);
    }

    /// 渲染纹理到窗口
    SDL_Rect rect = { 0, 0, scaleWidth(), scaleHeight() };
    ret           = SDL_RenderCopy(impl_->render_, impl_->texture_, NULL, &rect);
    if (ret != 0)
    {
        LOGE("SDL_RenderCopy failed: " << SDL_GetError());
        return false;
    }

    /// 显示渲染结果
    SDL_RenderPresent(impl_->render_);
    return true;
}
