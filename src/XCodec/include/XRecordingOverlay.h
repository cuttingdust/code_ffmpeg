#pragma once

#include "XOverlayStyle.h"

class XDisplayTask;
class XOpenGLVideoWidget;

/// 统一设置 SDL / OpenGL 两端的 REC overlay 样式
void applyOverlayStyle(const XOverlayStyle& style, XDisplayTask* display_task, XOpenGLVideoWidget* widget);

/// 统一开关 REC 指示器
void applyRecordingIndicator(bool show, XDisplayTask* display_task, XOpenGLVideoWidget* widget,
                             bool use_sdl_indicator, bool use_gl_indicator);
