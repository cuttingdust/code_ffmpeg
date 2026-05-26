#pragma once

#include "XTask.h"
#include "XVideoView.h"
#include "FrameWrapper.h"
#include <chrono>

class XDisplayTask : public XTask
{
    DECLARE_CREATE(XDisplayTask)
public:
    XDisplayTask();
    ~XDisplayTask() override;

    /// 自定义渲染回调
    using RenderCallback = std::function<void(FrameWrapper&)>;
    void setRenderCallback(RenderCallback cb);

    /// 首帧回调
    using FirstFrameCallback = std::function<void()>;
    void setFirstFrameCallback(FirstFrameCallback cb);

    /// 获取FPS统计
    int getFPS() const;

    /// 重置任务
    void reset() override;

    /// 获取视频渲染器
    XVideoView* getVideoView()
    {
        return view_.get();
    }

    /// 设置外部窗口句柄（必须在 start 之前调用）
    void setWindow(void* win);

protected:
    void process() override;

private:
    void defaultRender(FrameWrapper& frame);
    void updateFPS();
    void drawReconnectingMessage();

private:
    std::unique_ptr<XVideoView> view_;
    RenderCallback              render_cb_;
    FirstFrameCallback          first_frame_cb_;
    bool                        is_init_              = false;
    bool                        window_created_       = false;
    bool                        reconnecting_         = false;
    bool                        first_frame_received_ = false;
    void*                       external_win_         = nullptr; /// 外部窗口句柄

    std::chrono::steady_clock::time_point last_frame_time_;
    int                                   fps_         = 0;
    int                                   frame_count_ = 0;
    std::chrono::steady_clock::time_point last_stats_;
};
