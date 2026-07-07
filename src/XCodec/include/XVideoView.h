#ifndef XVIDEOVIEW_H
#define XVIDEOVIEW_H

#include "XCodec_Global.h"

#include <functional>
#include <memory>
#include <string>

struct AVFrame;

/// \brief 视频显示抽象基类（SDL 等后端实现）
///
/// 提供原始像素绘制、AVFrame 绘制、窗口/缩放与 YUV 文件读取等能力。
class XCODEC_EXPORT XVideoView
{
public:
    /// 支持的像素格式（与 FFmpeg AVPixelFormat 常用值对齐）
    enum Format
    {
        YUV420P = 0,
        NV12    = 23,
        ARGB    = 25,
        RGBA    = 26,
        BGRA    = 28
    };

    /// 渲染后端类型
    enum RenderType
    {
        SDL = 0
    };

    XVideoView();
    virtual ~XVideoView();

    /// \brief 按后端类型创建渲染器实例
    /// \param type 渲染后端，默认 SDL
    /// \return 渲染器指针，调用方负责 delete；不支持的后端返回 nullptr
    static auto create(RenderType type = SDL) -> XVideoView *;

    /// 叠加层绘制回调（如 REC 指示器），由具体后端在合适时机调用
    using OverlayCallback = std::function<void(void *renderer)>;

    /// \brief 设置叠加层回调
    virtual void setOverlayCallback(OverlayCallback cb);

public:
    /// \brief 初始化显示窗口/纹理
    /// \param w 视频宽度
    /// \param h 视频高度
    /// \param fmt 像素格式
    virtual auto init(int w, int h, Format fmt = RGBA) -> bool = 0;

    /// \brief 关闭并释放渲染资源
    virtual auto close() -> void = 0;

    /// \brief 重置渲染器（保留窗口，仅重建 renderer/texture）
    virtual auto resetRenderer() -> void = 0;

    /// \brief 是否收到退出事件（如 SDL_QUIT）
    virtual auto isExit() -> bool = 0;

    /// \brief 绘制单平面 RGB 或 NV12 打包数据
    /// \param data 像素数据首地址
    /// \param lineSize 行字节步长（pitch），0 时按格式自动推算
    virtual auto draw(const unsigned char *data, int lineSize = 0) -> bool = 0;

    /// \brief 绘制 YUV420P 三平面数据
    /// \param y,u,v 各平面首地址
    /// \param y_pitch,u_pitch,v_pitch 各平面行字节步长
    virtual auto draw(const unsigned char *y, int y_pitch, const unsigned char *u, int u_pitch, const unsigned char *v,
                      int v_pitch) -> bool = 0;

    /// \brief 根据 AVFrame 格式自动选择 draw 路径（YUV420P / NV12 / RGBA 等）
    /// \param frame FFmpeg 帧，需有效 data[0]
    /// \return 成功返回 true
    auto drawFrame(AVFrame *frame) -> bool;

    /// \brief 获取 SDL 渲染器指针（仅 XSDL 实现有效）
    virtual void *getSDLRenderer()
    {
        return nullptr;
    }

    /// \brief 同时设置显示缩放宽高
    void scale(int w, int h);

    /// \brief 设置显示缩放宽度（<=0 表示使用原始宽度）
    void setScaleWidth(int w);

    /// \brief 设置显示缩放高度（<=0 表示使用原始高度）
    void setScaleHeight(int h);

    /// \return 当前显示缩放宽度
    int scaleWidth();

    /// \return 当前显示缩放高度
    int scaleHeight();

    /// \return 最近统计周期内的渲染帧率
    int renderFps() const;

    /// \brief 打开原始 YUV/RGB 文件供 read() 顺序读取
    /// \param filepath 文件路径
    bool open(const std::string &filepath);

    /// \brief 从 open() 打开的文件读取下一帧
    /// \return AVFrame 指针（内部持有，勿 av_frame_free）；失败或 EOF 返回 nullptr
    AVFrame *read();

    /// \brief 绑定外部原生窗口句柄（HWND 等）
    void setWindow(void *win);

    /// \return 当前绑定的窗口句柄
    void *window() const;

    /// \return 是否已绑定外部窗口
    bool hasWin();

    /// \brief 设置视频原始宽度（read 模式使用）
    void setWidth(int width);

    /// \return 视频原始宽度
    int width() const;

    /// \brief 设置视频原始高度（read 模式使用）
    void setHeight(int height);

    /// \return 视频原始高度
    int height() const;

    /// \brief 设置 read 模式下的像素格式
    void setFormat(const Format &fmt);

    /// \return 当前像素格式
    Format format() const;

private:
    class PImpl;
    std::unique_ptr<PImpl> impl_;
};

#endif
