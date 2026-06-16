#pragma once

#include "XTask.h"
#include "XVideoView.h"
#include "FrameWrapper.h"
#include "XOverlayStyle.h"
#include <chrono>
#include <functional>
#include <SDL_pixels.h>

struct SDL_Texture;
struct TTF_Font;

/// REC 样式配置
struct RecStyle
{
    int       dot_radius     = 6;                      ///< 圆点半径
    SDL_Color dot_color      = { 255, 50, 50, 255 };   ///< 鲜红色圆点
    SDL_Color text_color     = { 255, 255, 255, 255 }; ///< 白色文字
    int       font_size      = 12;                     ///< 字体大小
    int       spacing        = 4;                      ///< 圆点和文字间距
    int       padding_top    = 2;                      ///< 上边距
    int       padding_bottom = 2;                      ///< 下边距
    int       padding_left   = 8;                      ///< 左边距
    int       padding_right  = 8;                      ///< 右边距
    bool      show_border    = true;                   ///< 是否显示边框
    SDL_Color border_color   = { 255, 255, 255, 200 }; ///< 边框颜色（半透明白）
};

/// \brief 视频显示 Task：从上游接收 AVFrame 并渲染
class XVideoDisplayTask : public XTask
{
    DECLARE_CREATE(XVideoDisplayTask)
public:
    XVideoDisplayTask();
    ~XVideoDisplayTask() override;

    using RenderCallback = std::function<void(FrameWrapper&)>;
    void setRenderCallback(RenderCallback cb);

    using FirstFrameCallback = std::function<void()>;
    void setFirstFrameCallback(FirstFrameCallback cb);

    int         getFPS() const;
    void        reset() override;
    XVideoView* getVideoView()
    {
        return view_.get();
    }
    void setWindow(void* win);

    void setRecordingIndicator(bool show);

    void setOverlayStyle(const XOverlayStyle& style);
    XOverlayStyle overlayStyle() const;

    void setPaused(bool paused) override;

protected:
    void process() override;


private:
    void defaultRender(FrameWrapper& frame);
    void updateFPS();
    void drawReconnectingMessage();
    void initRecTexture();
    void drawRecOverlay(void* renderer_ptr);
    void destroyRecTexture();

private:
    std::unique_ptr<XVideoView> view_;
    XOverlayStyle               overlay_style_;
    RecStyle                    rec_style_;
    RenderCallback              render_cb_;
    FirstFrameCallback          first_frame_cb_;
    bool                        is_init_              = false;
    bool                        window_created_       = false;
    bool                        reconnecting_         = false;
    bool                        first_frame_received_ = false;
    void*                       external_win_         = nullptr;
    std::atomic<bool>           show_rec_indicator_{ false };

    // REC 纹理相关
    void* rec_texture_        = nullptr; // SDL_Texture*
    int   rec_texture_width_  = 0;
    int   rec_texture_height_ = 0;
    void* rec_font_           = nullptr; // TTF_Font*

    std::chrono::steady_clock::time_point last_frame_time_;
    int                                   fps_         = 0;
    int                                   frame_count_ = 0;
    std::chrono::steady_clock::time_point last_stats_;
};
