#pragma once

#include "XTask.h"
#include "XVideoView.h"
#include "FrameWrapper.h"

class XDisplayTask : public XTask
{
    DECLARE_CREATE(XDisplayTask)
public:
    XDisplayTask();
    ~XDisplayTask() override;

public:
    /// 自定义渲染回调
    using RenderCallback = std::function<void(FrameWrapper&)>;

    void setRenderCallback(RenderCallback cb)
    {
        render_cb_ = cb;
    }

    /// 获取FPS统计
    int getFPS() const
    {
        return fps_;
    }

    /// 重置任务
    void reset() override;

protected:
    void process() override;

private:
    void defaultRender(FrameWrapper& frame);
    void updateFPS();
    void drawReconnectingMessage();

private:
    std::unique_ptr<XVideoView>           view_;
    RenderCallback                        render_cb_;
    bool                                  is_init_        = false;
    bool                                  window_created_ = false;
    bool                                  reconnecting_   = false;
    std::chrono::steady_clock::time_point last_frame_time_;

    // FPS统计
    int                                   fps_         = 0;
    int                                   frame_count_ = 0;
    std::chrono::steady_clock::time_point last_stats_;
};
