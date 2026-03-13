#pragma once

#include "XTask.h"
#include "XVideoView.h"

class XDisplayTask : public XTask
{
    DECLARE_CREATE(XDisplayTask)
public:
    XDisplayTask();
    ~XDisplayTask() override;

public:
    /// 自定义渲染回调
    using RenderCallback = std::function<void(AVFrame*)>;

    void setRenderCallback(RenderCallback cb)
    {
        render_cb_ = cb;
    }

    /// 获取FPS统计
    int getFPS() const
    {
        return fps_;
    }

protected:
    /// 任务处理逻辑
    void process() override;

private:
    void defaultRender(AVFrame* frame);
    void updateFPS();

private:
    std::unique_ptr<XVideoView> view_;
    RenderCallback              render_cb_;
    bool                        is_init_ = false;

    // FPS统计
    int                                   fps_         = 0;
    int                                   frame_count_ = 0;
    std::chrono::steady_clock::time_point last_stats_;
};
