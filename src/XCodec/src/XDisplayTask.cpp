#include "XDisplayTask.h"
#include "AVLog.h"
#include "FrameWrapper.h"
#include <sstream>
#include <utility>
#include <SDL.h>
#include <SDL_ttf.h>

XDisplayTask::XDisplayTask()
{
    setName("DisplayTask");
    LOGD("显示任务创建");
    last_stats_      = std::chrono::steady_clock::now();
    last_frame_time_ = std::chrono::steady_clock::now();
    rec_font_        = nullptr;
    rec_texture_     = nullptr;
}

XDisplayTask::~XDisplayTask()
{
    LOGD("显示任务销毁");
    destroyRecTexture();
}

void XDisplayTask::destroyRecTexture()
{
    if (rec_texture_)
    {
        SDL_DestroyTexture((SDL_Texture*)rec_texture_);
        rec_texture_ = nullptr;
    }
    if (rec_font_)
    {
        TTF_CloseFont((TTF_Font*)rec_font_);
        rec_font_ = nullptr;
    }
}

void XDisplayTask::setRenderCallback(RenderCallback cb)
{
    render_cb_ = std::move(cb);
}

void XDisplayTask::setFirstFrameCallback(FirstFrameCallback cb)
{
    first_frame_cb_ = std::move(cb);
}

void XDisplayTask::setRecordingIndicator(bool show)
{
    show_rec_indicator_ = show;
}

void XDisplayTask::setPaused(bool paused)
{
    XTask::setPaused(paused);
    if (!paused)
    {
        last_frame_time_ = std::chrono::steady_clock::now();
        last_stats_      = std::chrono::steady_clock::now();
        frame_count_     = 0;
    }
}

int XDisplayTask::getFPS() const
{
    return fps_;
}

void XDisplayTask::setWindow(void* win)
{
    external_win_ = win;
    LOGI("设置外部窗口句柄: " << win);
}

void XDisplayTask::reset()
{
    XTask::reset();

    LOGD("重置显示任务");

    if (view_)
    {
        view_->resetRenderer();
    }

    is_init_              = false;
    fps_                  = 0;
    frame_count_          = 0;
    last_stats_           = std::chrono::steady_clock::now();
    last_frame_time_      = std::chrono::steady_clock::now();
    reconnecting_         = true;
    first_frame_received_ = false;
    show_rec_indicator_   = false;

    destroyRecTexture();
}

void XDisplayTask::initRecTexture()
{
    if (!view_)
    {
        LOGE("initRecTexture: view_ 为空");
        return;
    }

    SDL_Renderer* renderer = (SDL_Renderer*)view_->getSDLRenderer();
    if (!renderer)
    {
        LOGE("initRecTexture: 获取渲染器失败");
        return;
    }

    // 初始化 TTF
    if (TTF_Init() == -1)
    {
        LOGE("TTF_Init 失败: " << TTF_GetError());
        return;
    }

    // 加载字体
    rec_font_ = TTF_OpenFont("C:/Windows/Fonts/arial.ttf", rec_style_.font_size);
    if (!rec_font_)
    {
        rec_font_ = TTF_OpenFont("C:/Windows/Fonts/msyh.ttc", rec_style_.font_size);
    }
    if (!rec_font_)
    {
        LOGE("打开字体失败: " << TTF_GetError());
        return;
    }

    /// 创建文字表面
    SDL_Surface* text_surface = TTF_RenderUTF8_Blended((TTF_Font*)rec_font_, "REC", rec_style_.text_color);
    if (!text_surface)
    {
        LOGE("创建文字表面失败: " << TTF_GetError());
        return;
    }

    /// 计算组合纹理尺寸
    int dot_size = rec_style_.dot_radius * 2;
    int total_width =
            rec_style_.padding_left + dot_size + rec_style_.spacing + text_surface->w + rec_style_.padding_right;
    int total_height = std::max(dot_size, text_surface->h) + rec_style_.padding_top + rec_style_.padding_bottom;

    /// 创建组合表面（带透明通道）
    SDL_Surface* combined = SDL_CreateRGBSurfaceWithFormat(0, total_width, total_height, 32, SDL_PIXELFORMAT_RGBA32);
    if (!combined)
    {
        LOGE("创建组合表面失败");
        SDL_FreeSurface(text_surface);
        return;
    }

    /// 填充透明背景
    SDL_FillRect(combined, NULL, SDL_MapRGBA(combined->format, 0, 0, 0, 0));

    // ========== 绘制圆点 ==========
    SDL_LockSurface(combined);
    Uint32 dot_color = SDL_MapRGBA(combined->format, rec_style_.dot_color.r, rec_style_.dot_color.g,
                                   rec_style_.dot_color.b, rec_style_.dot_color.a);

    int dot_center_x = rec_style_.padding_left + rec_style_.dot_radius;
    int dot_center_y = rec_style_.padding_top + dot_size / 2;

    /// 绘制实心圆
    for (int y = -rec_style_.dot_radius; y <= rec_style_.dot_radius; y++)
    {
        for (int x = -rec_style_.dot_radius; x <= rec_style_.dot_radius; x++)
        {
            if (x * x + y * y <= rec_style_.dot_radius * rec_style_.dot_radius)
            {
                int px = dot_center_x + x;
                int py = dot_center_y + y;
                if (px >= 0 && px < combined->w && py >= 0 && py < combined->h)
                {
                    ((Uint32*)combined->pixels)[py * combined->w + px] = dot_color;
                }
            }
        }
    }
    SDL_UnlockSurface(combined);

    // ========== 绘制文字 ==========
    SDL_Rect text_rect = {
        rec_style_.padding_left + dot_size + rec_style_.spacing,
        rec_style_.padding_top +
                (total_height - rec_style_.padding_top - rec_style_.padding_bottom - text_surface->h) / 2,
        text_surface->w, text_surface->h
    };
    SDL_BlitSurface(text_surface, NULL, combined, &text_rect);

    // ========== 绘制边框 ==========
    if (rec_style_.show_border)
    {
        SDL_LockSurface(combined);
        Uint32 border_color = SDL_MapRGBA(combined->format, rec_style_.border_color.r, rec_style_.border_color.g,
                                          rec_style_.border_color.b, rec_style_.border_color.a);

        for (int x = 0; x < combined->w; x++)
        {
            ((Uint32*)combined->pixels)[0 * combined->w + x]                 = border_color;
            ((Uint32*)combined->pixels)[(combined->h - 1) * combined->w + x] = border_color;
        }
        for (int y = 0; y < combined->h; y++)
        {
            ((Uint32*)combined->pixels)[y * combined->w + 0]                 = border_color;
            ((Uint32*)combined->pixels)[y * combined->w + (combined->w - 1)] = border_color;
        }
        SDL_UnlockSurface(combined);
    }

    // 创建纹理
    rec_texture_ = SDL_CreateTextureFromSurface(renderer, combined);
    if (rec_texture_)
    {
        rec_texture_width_  = combined->w;
        rec_texture_height_ = combined->h;
        LOGI("REC 组合纹理创建成功: " << rec_texture_width_ << "x" << rec_texture_height_);
    }
    else
    {
        LOGE("创建纹理失败: " << SDL_GetError());
    }

    SDL_FreeSurface(text_surface);
    SDL_FreeSurface(combined);
}

void XDisplayTask::drawRecOverlay(void* renderer_ptr)
{
    SDL_Renderer* renderer = (SDL_Renderer*)renderer_ptr;
    if (!renderer || !show_rec_indicator_)
    {
        return;
    }

    if (!rec_texture_)
    {
        initRecTexture();
    }

    if (rec_texture_)
    {
        SDL_Rect dst_rect = { 8, 8, rec_texture_width_, rec_texture_height_ };
        SDL_RenderCopy(renderer, (SDL_Texture*)rec_texture_, nullptr, &dst_rect);
    }
}

void XDisplayTask::updateFPS()
{
    frame_count_++;
    auto now     = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_stats_).count();

    if (elapsed >= 1)
    {
        fps_         = frame_count_;
        frame_count_ = 0;
        last_stats_  = now;
        LOGI("当前FPS: " << fps_);
    }
}

void XDisplayTask::drawReconnectingMessage()
{
    if (!view_ || !window_created_)
    {
        return;
    }

    static int counter = 0;
    counter++;

    if (counter % 30 == 0)
    {
        LOGI("网络断开，等待重连...");
    }
}

void XDisplayTask::defaultRender(FrameWrapper& frame)
{
    if (!view_)
    {
        view_.reset(XVideoView::create());
        if (!view_)
        {
            LOGE("创建渲染器失败");
            return;
        }
        LOGD("渲染器创建成功");

        if (external_win_)
        {
            view_->setWindow(external_win_);
            LOGI("设置外部窗口: " << external_win_);
        }

        view_->setOverlayCallback(
                [this](void* renderer)
                {
                    if (show_rec_indicator_)
                    {
                        drawRecOverlay(renderer);
                    }
                });
    }

    if (!window_created_ && frame->width > 0 && frame->height > 0)
    {
        LOGI("尝试在子线程初始化窗口: " << frame->width << "x" << frame->height);
        bool result = view_->init(frame->width, frame->height, (XVideoView::Format)frame->format);
        if (result)
        {
            window_created_ = true;
            is_init_        = true;
            LOGI("窗口初始化成功");
        }
        else
        {
            LOGE("窗口初始化失败");
            return;
        }
    }

    if (window_created_ && !is_init_)
    {
        LOGI("重新初始化渲染器");
        bool result = view_->init(frame->width, frame->height, (XVideoView::Format)frame->format);
        if (result)
        {
            is_init_ = true;
            LOGI("渲染器重新初始化成功");
        }
        else
        {
            LOGE("渲染器重新初始化失败");
            return;
        }
    }

    if (is_init_ && view_)
    {
        if (!first_frame_received_)
        {
            first_frame_received_ = true;
            if (first_frame_cb_)
            {
                first_frame_cb_();
            }
        }

        bool draw_result = view_->drawFrame(frame);
        if (!draw_result)
        {
            LOGE("drawFrame 失败");
        }

        if (reconnecting_)
        {
            drawReconnectingMessage();
        }

        if (reconnecting_)
        {
            LOGI("网络已恢复，继续播放");
            reconnecting_ = false;
        }

        last_frame_time_ = std::chrono::steady_clock::now();
        updateFPS();
    }
}

void XDisplayTask::process()
{
    LOGI("显示任务开始运行");

    int           consecutive_timeouts     = 0;
    constexpr int max_consecutive_timeouts = 300;

    int           initial_wait     = 0;
    constexpr int max_initial_wait = 200;

    // // ✅ 帧率控制变量（使用 sleep_until 精确控制）
    // auto      next_frame_time   = std::chrono::steady_clock::now();
    // const int frame_interval_ms = 40; // 25fps = 40ms

    while (!shouldStop())
    {
        /// 检查暂停
        if (shouldPause())
        {
            continue;
        }

        AVFrame* raw_frame = popFrame();

        auto now       = std::chrono::steady_clock::now();
        auto idle_time = std::chrono::duration_cast<std::chrono::seconds>(now - last_frame_time_).count();

        if (idle_time > 3 && window_created_ && !reconnecting_ &&
            last_frame_time_ != std::chrono::steady_clock::time_point{})
        {
            reconnecting_ = true;
            LOGI("检测到网络断开，进入重连状态");
        }


        if (!raw_frame)
        {
            if (initial_wait < max_initial_wait && last_frame_time_ == std::chrono::steady_clock::time_point{})
            {
                initial_wait++;
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                continue;
            }

            consecutive_timeouts++;

            if (consecutive_timeouts >= max_consecutive_timeouts)
            {
                LOGE("显示任务连续超时 " << consecutive_timeouts << " 次，触发重连");
                handleError("上游可能卡死");
                break;
            }

            if (shouldStop() || (eof_reached_ && frame_queue_.empty()))
            {
                break;
            }

            if (reconnecting_)
            {
                drawReconnectingMessage();
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        initial_wait         = 0;
        consecutive_timeouts = 0;

        // // ✅ 使用 sleep_until 精确控制帧间隔
        // next_frame_time += std::chrono::milliseconds(frame_interval_ms);
        // std::this_thread::sleep_until(next_frame_time);

        FrameWrapper frame(raw_frame);

        if (render_cb_)
        {
            render_cb_(frame);
        }
        else
        {
            defaultRender(frame);
        }
    }

    LOGI("显示任务结束");
}

IMPLEMENT_CREATE(XDisplayTask)
