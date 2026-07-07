#ifndef XSDL_H
#define XSDL_H

#include "XVideoView.h"

/// \brief 基于 SDL2 的 XVideoView 实现
///
/// 支持外部窗口嵌入（setWindow）或自建 SDL 窗口，使用纹理上传 + RenderCopy 显示
/// YUV420P / NV12 / RGBA 等格式。析构时自动 close() 释放 SDL 资源。
class XSDL : public XVideoView
{
public:
    XSDL();
    ~XSDL() override;

    /// \brief 设置叠加层回调（在 Present 前调用，用于 REC 等 overlay）
    void setOverlayCallback(OverlayCallback cb) override;

public:
    /// \brief 初始化 SDL 视频子系统并创建/复用窗口与纹理
    /// \param w 视频宽度
    /// \param h 视频高度
    /// \param fmt 像素格式，映射为对应 SDL_PIXELFORMAT_*
    /// \return 成功返回 true
    auto init(int w, int h, Format fmt) -> bool override;

    /// \brief 轮询 SDL 事件，判断是否收到退出请求
    auto isExit() -> bool override;

    /// \brief 销毁纹理、渲染器与窗口，并标记 SDL 子系统可退出
    auto close() -> void override;

    /// \brief 仅销毁 renderer/texture，保留 SDL 窗口（分辨率或格式变更时复用）
    auto resetRenderer() -> void override;

    /// \brief 上传并绘制单平面数据（RGB32 / NV12 打包缓冲等）
    /// \param data 像素首地址
    /// \param lineSize 行字节步长（pitch）
    auto draw(const unsigned char *data, int lineSize) -> bool override;

    /// \brief 上传并绘制 YUV420P 三平面数据（SDL_UpdateYUVTexture）
    /// \param y,u,v 各平面首地址
    /// \param y_pitch,u_pitch,v_pitch 各平面行字节步长
    auto draw(const unsigned char *y, int y_pitch, const unsigned char *u, int u_pitch, const unsigned char *v,
              int v_pitch) -> bool override;

    /// \return SDL_Renderer*，供 XVideoDisplayTask 等创建叠加纹理
    void *getSDLRenderer() override;

private:
    class PImpl;
    std::unique_ptr<PImpl> impl_;
};

#endif
