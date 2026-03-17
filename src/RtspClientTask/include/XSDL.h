#ifndef XSDL_H
#define XSDL_H

#include "XVideoView.h"

/**
 * @brief SDL2渲染器实现类
 * 
 * 继承自XVideoView，使用SDL2库实现视频渲染功能
 * 支持：
 * - 窗口创建和管理
 * - 多线程安全渲染
 * - 多种像素格式（YUV420P、NV12、RGBA等）
 * - 窗口永久存活，重连时只重置渲染器
 */
class XSDL : public XVideoView
{
public:
    XSDL();
    ~XSDL() override;

public:
    /**
     * @brief 初始化SDL渲染窗口
     * @param w 窗口宽度
     * @param h 窗口高度
     * @param fmt 像素格式
     * @return 成功返回true，失败返回false
     * @note 窗口只创建一次，后续调用只会重置渲染器
     */
    auto init(int w, int h, Format fmt) -> bool override;

    /**
     * @brief 检查窗口退出事件
     * @return 用户点击关闭窗口返回true
     */
    auto isExit() -> bool override;

    /**
     * @brief 关闭窗口并释放资源
     * @note 由于窗口永久存活策略，此方法只重置渲染器
     */
    auto close() -> void override;

    /**
     * @brief 重置渲染器
     * @note 保留窗口，只销毁并重新创建渲染器和纹理
     *       在重连或流切换时调用，避免窗口闪烁
     */
    auto resetRenderer() -> void override;

    /**
     * @brief 渲染RGB/ARGB等单平面数据
     * @param data 图像数据指针
     * @param lineSize 一行数据的字节数
     * @return 成功返回true，失败返回false
     */
    auto draw(const unsigned char *data, int lineSize) -> bool override;

    /**
     * @brief 渲染YUV420P格式数据
     * @param y Y平面数据指针
     * @param y_pitch Y平面一行字节数
     * @param u U平面数据指针
     * @param u_pitch U平面一行字节数
     * @param v V平面数据指针
     * @param v_pitch V平面一行字节数
     * @return 成功返回true，失败返回false
     */
    auto draw(const unsigned char *y, int y_pitch, const unsigned char *u, int u_pitch, const unsigned char *v,
              int v_pitch) -> bool override;

private:
    class PImpl;
    std::unique_ptr<PImpl> impl_;
};

#endif // XSDL_H
