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

/// \brief SDL 路径下 REC 叠加层样式（由 XOverlayStyle 转换而来）
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

/// \brief 视频显示 Task：从上游队列取 AVFrame 并渲染
///
/// 默认使用 XSDL 后端；可通过 setRenderCallback 接管渲染（如 OpenGL 主线程绘制）。
/// 支持首帧回调、断流重连提示、REC 录制指示叠加（SDL + TTF）。
class XVideoDisplayTask : public XTask
{
    DECLARE_CREATE(XVideoDisplayTask)
public:
    XVideoDisplayTask();
    ~XVideoDisplayTask() override;

    /// 自定义渲染回调；设置后不再走 defaultRender（SDL drawFrame）
    using RenderCallback = std::function<void(FrameWrapper&)>;

    /// \brief 设置帧渲染回调（用于 OpenGL 等外部渲染路径）
    void setRenderCallback(RenderCallback cb);

    /// 首帧成功显示时触发（常用于隐藏 loading、调整窗口）
    using FirstFrameCallback = std::function<void()>;

    /// \brief 设置首帧回调
    void setFirstFrameCallback(FirstFrameCallback cb);

    /// \return 最近统计周期内的显示帧率
    int getFPS() const;

    /// \brief 重置任务：清空队列、重置 SDL 视图与 overlay 状态
    void reset() override;

    /// \return 内部 XVideoView（XSDL），未初始化时可能为 nullptr
    XVideoView* getVideoView()
    {
        return view_.get();
    }

    /// \brief 绑定外部原生窗口句柄，传给 XSDL::setWindow
    /// \param win HWND 等，需在 init 前或首帧前设置
    void setWindow(void* win);

    /// \brief 是否显示左上角 REC 录制指示（SDL overlay 路径）
    void setRecordingIndicator(bool show);

    /// \brief 设置叠加层样式，会销毁并延迟重建 REC 纹理
    void setOverlayStyle(const XOverlayStyle& style);

    /// \return 当前叠加层样式
    XOverlayStyle overlayStyle() const;

    /// \brief 暂停/恢复显示；恢复时重置 FPS 统计起点
    void setPaused(bool paused) override;

protected:
    /// \brief 工作线程主循环：popFrame → 渲染 → 更新 FPS / 断流检测
    void process() override;

private:
    /// 默认 SDL 渲染：lazy init XSDL，drawFrame，可选 REC overlay
    void defaultRender(FrameWrapper& frame);

    /// 按秒统计并更新 fps_
    void updateFPS();

    /// 断流重连时在画面上绘制提示文字
    void drawReconnectingMessage();

    /// 根据 rec_style_ 创建 REC 组合纹理（SDL_Texture + TTF）
    void initRecTexture();

    /// 将 REC 纹理绘制到当前 SDL_Renderer
    void drawRecOverlay(void* renderer_ptr);

    /// 释放 REC 纹理、字体及 TTF 子系统引用
    void destroyRecTexture();

private:
    std::unique_ptr<XVideoView> view_; ///< 默认 XSDL 实例
    XOverlayStyle               overlay_style_;
    RecStyle                    rec_style_;
    RenderCallback              render_cb_;
    FirstFrameCallback          first_frame_cb_;
    bool                        is_init_              = false; ///< XSDL 是否已完成 init
    bool                        window_created_       = false; ///< 是否已创建/绑定窗口
    bool                        reconnecting_         = false; ///< 是否处于断流重连状态
    bool                        first_frame_received_ = false;
    void*                       external_win_         = nullptr; ///< 外部窗口句柄
    std::atomic<bool>           show_rec_indicator_{ false };

    void* rec_texture_         = nullptr; ///< SDL_Texture*，REC 组合图
    int   rec_texture_width_   = 0;
    int   rec_texture_height_  = 0;
    void* rec_font_            = nullptr; ///< TTF_Font*
    bool  rec_ttf_initialized_ = false;   ///< 本 Task 是否调用过 TTF_Init

    std::chrono::steady_clock::time_point last_frame_time_; ///< 上一帧时间，用于断流检测
    int                                   fps_         = 0;
    int                                   frame_count_ = 0;
    std::chrono::steady_clock::time_point last_stats_; ///< FPS 统计窗口起点
};
