#ifndef XVIDEOVIEW_H
#define XVIDEOVIEW_H

#include <memory>
#include <string>

struct AVFrame;

/**
 * @brief 视频渲染器抽象基类
 * 
 * 提供统一的视频渲染接口，支持多种渲染后端（如SDL）
 * 负责视频帧的显示、窗口管理、FPS统计等功能
 */
class XVideoView
{
public:
    /**
     * @brief 像素格式枚举
     * @note 枚举值与FFmpeg中的AVPixelFormat保持一致
     */
    enum Format
    {
        YUV420P = 0,  ///< YUV 4:2:0 平面格式，每个像素12位
        NV12    = 23, ///< YUV 4:2:0 半平面格式，Y平面 + UV交错平面
        ARGB    = 25, ///< 32位ARGB格式（高字节是Alpha）
        RGBA    = 26, ///< 32位RGBA格式（低字节是Alpha）
        BGRA    = 28  ///< 32位BGRA格式
    };

    /**
     * @brief 渲染后端类型
     */
    enum RenderType
    {
        SDL = 0 ///< 使用SDL2渲染后端
    };

    XVideoView();
    virtual ~XVideoView();

public:
    /**
     * @brief 创建视频渲染器实例
     * @param type 渲染后端类型，默认为SDL
     * @return 渲染器指针，失败返回nullptr
     */
    static auto create(RenderType type = SDL) -> XVideoView *;

    /**
     * @brief 初始化渲染窗口
     * @param w 窗口宽度
     * @param h 窗口高度
     * @param fmt 像素格式，默认为RGBA
     * @return 成功返回true，失败返回false
     * @note 这个方法只应该调用一次，用于创建窗口
     */
    virtual auto init(int w, int h, Format fmt = RGBA) -> bool = 0;

    /**
     * @brief 清理所有申请的资源，包括关闭窗口
     */
    virtual auto close() -> void = 0;

    /**
     * @brief 重置渲染器
     * @note 保留窗口，只重置渲染相关的资源
     *       在重连或流切换时调用，避免重新创建窗口
     */
    virtual auto resetRenderer() -> void = 0;

    /**
     * @brief 处理窗口退出事件
     * @return 用户点击关闭窗口返回true，否则返回false
     */
    virtual auto isExit() -> bool = 0;

    /**
     * @brief 渲染RGB/ARGB等单平面数据
     * @param data 图像数据指针
     * @param lineSize 一行数据的字节数，为0时会根据宽度和格式自动计算
     * @return 成功返回true，失败返回false
     */
    virtual auto draw(const unsigned char *data, int lineSize = 0) -> bool = 0;

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
    virtual auto draw(const unsigned char *y, int y_pitch, const unsigned char *u, int u_pitch, const unsigned char *v,
                      int v_pitch) -> bool = 0;

    /**
     * @brief 渲染AVFrame格式的帧
     * @param frame FFmpeg AVFrame指针
     * @return 成功返回true，失败返回false
     * @note 自动识别帧格式，支持YUV420P、NV12、BGRA、ARGB、RGBA
     */
    auto drawFrame(AVFrame *frame) -> bool;

public:
    // ==================== 缩放控制 ====================

    /**
     * @brief 设置显示缩放大小
     * @param w 显示宽度
     * @param h 显示高度
     */
    auto scale(int w, int h) -> void;

    /** @brief 设置显示宽度 */
    auto setScaleWidth(int w) -> void;

    /** @brief 设置显示高度 */
    auto setScaleHeight(int h) -> void;

    /** @brief 获取显示宽度 */
    auto scaleWidth() -> int;

    /** @brief 获取显示高度 */
    auto scaleHeight() -> int;

    // ==================== 统计信息 ====================

    /**
     * @brief 获取渲染帧率
     * @return 当前渲染FPS
     */
    auto renderFps() const -> int;

    // ==================== 文件操作 ====================

    /**
     * @brief 打开视频文件（用于直接读取帧）
     * @param filepath 文件路径
     * @return 成功返回true，失败返回false
     */
    auto open(const std::string &filepath) -> bool;

    /**
     * @brief 读取一帧数据
     * @return AVFrame指针，失败返回nullptr
     * @note 每次调用会覆盖上一次的数据
     */
    auto read() -> AVFrame *;

    // ==================== 窗口管理 ====================

    /**
     * @brief 设置外部窗口句柄
     * @param win 窗口句柄（平台相关）
     */
    auto setWindow(void *win) -> void;

    /** @brief 获取窗口句柄 */
    auto window() const -> void *;

    /** @brief 检查是否设置了外部窗口 */
    auto hasWin() -> bool;

    // ==================== 属性设置 ====================

    /** @brief 设置视频宽度 */
    auto setWidth(int width) -> void;

    /** @brief 获取视频宽度 */
    auto width() const -> int;

    /** @brief 设置视频高度 */
    auto setHeight(int height) -> void;

    /** @brief 获取视频高度 */
    auto height() const -> int;

    /** @brief 设置像素格式 */
    auto setFormat(const Format &fmt) -> void;

    /** @brief 获取像素格式 */
    auto format() const -> Format;

private:
    class PImpl;
    std::unique_ptr<PImpl> impl_;
};

#endif // XVIDEOVIEW_H
