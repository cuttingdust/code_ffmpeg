#pragma once

#include "FrameWrapper.h"
#include "XDisplayTask.h"
#include "XOpenGLVideoWidget.h"

/// 将 DisplayTask 绑定到 Qt OpenGL 控件（主线程渲染）
inline void bindOpenGLDisplayTask(XDisplayTask* task, XOpenGLVideoWidget* widget)
{
    if (!task || !widget)
    {
        return;
    }

    task->setRenderCallback(
            [widget](const FrameWrapper& frame)
            {
                if (frame)
                {
                    widget->submitFrame(frame.get());
                }
            });
}

/// 恢复 SDL 默认渲染
inline void bindSdlDisplayTask(XDisplayTask* task, void* win_id)
{
    if (!task)
    {
        return;
    }

    task->setRenderCallback(nullptr);
    task->setWindow(win_id);
}
