#include "XRecordingOverlay.h"

#include "XVideoDisplayTask.h"
#include "XOpenGLVideoWidget.h"

void applyOverlayStyle(const XOverlayStyle& style, XVideoDisplayTask* display_task, XOpenGLVideoWidget* widget)
{
    if (display_task)
    {
        display_task->setOverlayStyle(style);
    }
    if (widget)
    {
        widget->setOverlayStyle(style);
    }
}

void applyRecordingIndicator(bool show, XVideoDisplayTask* display_task, XOpenGLVideoWidget* widget, bool use_sdl_indicator,
                             bool use_gl_indicator)
{
    if (use_gl_indicator && widget)
    {
        widget->setRecordingIndicator(show);
    }
    if (use_sdl_indicator && display_task)
    {
        display_task->setRecordingIndicator(show);
    }
}
