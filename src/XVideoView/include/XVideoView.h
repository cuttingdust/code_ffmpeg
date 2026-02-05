#ifndef XVIDEOVIEW_H
#define XVIDEOVIEW_H

#include <memory>

class XVideoView
{
public:
    enum Format
    {
        RGBA = 0,
        ARGB,
        YUV420P
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
    /// \param win_id 窗口句柄，如果为空，创建新窗口
    /// \return 是否创建成功
    virtual auto init(int w, int h, Format fmt = RGBA, void *win_id = nullptr) -> bool = 0;

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

public:
    auto scale(int w, int h) -> void;

    auto setScaleWidth(int w) -> void;

    auto setScaleHeight(int h) -> void;

    auto scaleWith() -> int;

    auto scaleHeight() -> int;

private:
    class PImpl;
    std::unique_ptr<PImpl> impl_;
};


#endif // XVIDEOVIEW_H
