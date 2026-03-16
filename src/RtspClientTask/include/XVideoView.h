#ifndef XVIDEOVIEW_H
#define XVIDEOVIEW_H

#include <memory>
#include <string>

struct AVFrame;

class XVideoView
{
public:
    enum Format
    {
        YUV420P = 0,
        NV12    = 23,
        ARGB    = 25,
        RGBA    = 26,
        BGRA    = 28
    };
    enum RenderType
    {
        SDL = 0
    };
    XVideoView();
    virtual ~XVideoView();

public:
    static auto create(RenderType type = SDL) -> XVideoView *;

    /// 初始化渲染窗口（只调用一次）
    virtual auto init(int w, int h, Format fmt = RGBA) -> bool = 0;

    /// 清理所有申请的资源，包括关闭窗口
    virtual auto close() -> void = 0;

    /// ✅ 重置渲染器（保留窗口，只重置渲染相关资源）
    virtual auto resetRenderer() -> void = 0;

    /// 处理窗口退出事件
    virtual auto isExit() -> bool = 0;

    /// 渲染图像
    virtual auto draw(const unsigned char *data, int lineSize = 0) -> bool = 0;
    virtual auto draw(const unsigned char *y, int y_pitch, const unsigned char *u, int u_pitch, const unsigned char *v,
                      int v_pitch) -> bool                                 = 0;

    auto drawFrame(AVFrame *frame) -> bool;

public:
    auto scale(int w, int h) -> void;
    auto setScaleWidth(int w) -> void;
    auto setScaleHeight(int h) -> void;
    auto scaleWidth() -> int;
    auto scaleHeight() -> int;
    auto renderFps() const -> int;
    auto open(const std::string &filepath) -> bool;
    auto read() -> AVFrame *;
    auto setWindow(void *win) -> void;
    auto window() const -> void *;
    auto hasWin() -> bool;
    auto setWidth(int width) -> void;
    auto width() const -> int;
    auto setHeight(int height) -> void;
    auto height() const -> int;
    auto setFormat(const Format &fmt) -> void;
    auto format() const -> Format;

private:
    class PImpl;
    std::unique_ptr<PImpl> impl_;
};

#endif // XVIDEOVIEW_H
