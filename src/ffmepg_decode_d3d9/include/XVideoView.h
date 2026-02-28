#ifndef XVIDEOVIEW_H
#define XVIDEOVIEW_H

#include <memory>
#include <string>

struct AVFrame;

class XVideoView
{
public:
    enum Format /// 枚举的值和ffmpeg中一致
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

    /// \brief 初始化渲染窗口 线程安全
    /// \param w  窗口宽度
    /// \param h  窗口高度
    /// \param fmt  绘制的像素格式
    /// \return 是否创建成功
    virtual auto init(int w, int h, Format fmt = RGBA) -> bool = 0;

    /// \brief 清理所有申请的资源，包括关闭窗口
    virtual auto close() -> void = 0;

    /// \brief 处理窗口退出事件
    /// \return
    virtual auto isExit() -> bool = 0;

    /// \brief 渲染图像 线程安全
    /// \param data 渲染的二进制数据
    /// \param lineSize 一行数据的字节数，对于YUV420P就是Y一行字节数
    /// \note  lineSize<=0 就根据宽度和像素格式自动算出大小
    /// \return 渲染是否成功
    virtual auto draw(const unsigned char *data, int lineSize = 0) -> bool = 0;

    virtual auto draw(const unsigned char *y, int y_pitch, const unsigned char *u, int u_pitch, const unsigned char *v,
                      int v_pitch) -> bool = 0;

    auto drawFrame(AVFrame *frame) -> bool;

public:
    auto scale(int w, int h) -> void;

    auto setScaleWidth(int w) -> void;

    auto setScaleHeight(int h) -> void;

    auto scaleWidth() -> int;

    auto scaleHeight() -> int;

    auto renderFps() const -> int;

    /// \brief 打开文件
    /// \param filepath
    /// \return
    auto open(const std::string &filepath) -> bool;

    /// \brief 读取一帧数据，并维护AVFrame空间
    /// \note 每次调用会覆盖上一次数据
    /// \return
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
