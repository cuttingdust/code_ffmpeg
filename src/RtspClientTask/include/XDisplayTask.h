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

    auto setRenderCallback(RenderCallback cb) -> void;

    /// 获取FPS统计
    auto getFPS() const -> int;

    /// 重置任务
    auto reset() -> void override;

protected:
    auto process() -> void override;

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
