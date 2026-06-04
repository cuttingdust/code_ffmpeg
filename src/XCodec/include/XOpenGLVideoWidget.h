#pragma once

#include "XCodec_Global.h"
#include "XOverlayStyle.h"

#include <QtOpenGLWidgets/QOpenGLWidget>
#include <QtGui/QOpenGLFunctions>
#include <QtOpenGL/QOpenGLShaderProgram>
#include <QtOpenGL/QOpenGLVertexArrayObject>
#include <QtOpenGL/QOpenGLBuffer>
#include <QtOpenGL/QOpenGLTexture>

#include <QtCore/QElapsedTimer>
#include <QtCore/QString>

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

struct AVFrame;

class XCODEC_EXPORT XOpenGLVideoWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT

public:
    enum class FramePixelFormat
    {
        YUV420P = 0,
        NV12,
        RGBA,
        BGRA,
        ARGB
    };

    enum class ScaleMode
    {
        Stretch = 0,
        KeepAspectRatio
    };

    enum class FrameUpdatePolicy
    {
        CoalesceLatest = 0,
        DropWhenBusy,
        QueueEveryFrame
    };

    explicit XOpenGLVideoWidget(QWidget* parent = nullptr);
    ~XOpenGLVideoWidget() override;

    /// 确保 OpenGL 资源已创建（通常由 initializeGL 自动完成；播放前可主动调用）
    bool init();
    bool isInit() const;

    using FirstFrameCallback = std::function<void()>;
    void setFirstFrameCallback(FirstFrameCallback cb);

    void submitFrame(const AVFrame* frame);

    void      setScaleMode(ScaleMode mode);
    ScaleMode scaleMode() const;

    void scale(int w, int h);
    void setScaleWidth(int w);
    void setScaleHeight(int h);
    int  scaleWidth() const;
    int  scaleHeight() const;

    void              setFrameUpdatePolicy(FrameUpdatePolicy policy);
    FrameUpdatePolicy frameUpdatePolicy() const;

    void setRecordingIndicator(bool show);
    bool recordingIndicator() const;

    void    setOverlayMessage(const QString& message);
    QString overlayMessage() const;

    void          setOverlayStyle(const XOverlayStyle& style);
    XOverlayStyle overlayStyle() const;

    int videoWidth() const;
    int videoHeight() const;
    int renderFps() const;
    int droppedFrames() const;

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

private:
    struct FrameBuffer
    {
        int                  width  = 0;
        int                  height = 0;
        FramePixelFormat     format = FramePixelFormat::YUV420P;
        std::vector<uint8_t> y;
        std::vector<uint8_t> u;
        std::vector<uint8_t> v;
        std::vector<uint8_t> uv;
        std::vector<uint8_t> rgba;
    };

    struct PlanePbo
    {
        QOpenGLBuffer buffers[2]  = { QOpenGLBuffer(QOpenGLBuffer::PixelUnpackBuffer),
                                      QOpenGLBuffer(QOpenGLBuffer::PixelUnpackBuffer) };
        int           capacity    = 0;
        int           write_index = 0;

        QOpenGLBuffer& acquire()
        {
            QOpenGLBuffer& buf = buffers[write_index];
            write_index        = 1 - write_index;
            return buf;
        }

        void destroy()
        {
            for (auto& buffer : buffers)
            {
                if (buffer.isCreated())
                {
                    buffer.destroy();
                }
            }
            capacity    = 0;
            write_index = 0;
        }
    };

    void initGlResources();
    void initShaders();
    void initGeometry();
    void ensureTextures(int width, int height, FramePixelFormat format);
    void destroyTextures();
    void destroyPbos();
    void bindGeometry(QOpenGLShaderProgram& program);
    void updateVideoVertices(int video_width, int video_height);

    static void copyYuv420p(const AVFrame* frame, FrameBuffer& out);
    static void copyNv12(const AVFrame* frame, FrameBuffer& out);
    static void copyRgba(const AVFrame* frame, FrameBuffer& out);
    static void copyBgra(const AVFrame* frame, FrameBuffer& out);
    static void copyArgb(const AVFrame* frame, FrameBuffer& out);
    static void copyRgb24(const AVFrame* frame, FrameBuffer& out);
    static void copyBgr24(const AVFrame* frame, FrameBuffer& out);
    static bool convertToYuv420p(const AVFrame* frame, FrameBuffer& out);

    void uploadTextureData(QOpenGLTexture* texture, const uint8_t* data, int width, int height,
                           QOpenGLTexture::PixelFormat pixel_format, PlanePbo& pbo);

    void drawYuv420pFrame(const FrameBuffer& frame);
    void drawNv12Frame(const FrameBuffer& frame);
    void drawRgbaFrame(const FrameBuffer& frame);
    void drawOverlays();
    void updateRenderFps();

    QOpenGLShaderProgram     program_yuv420p_;
    QOpenGLShaderProgram     program_nv12_;
    QOpenGLShaderProgram     program_rgba_;
    QOpenGLVertexArrayObject vao_;
    QOpenGLBuffer            vbo_vertices_;
    QOpenGLBuffer            vbo_texcoords_;

    QOpenGLTexture* texture_y_    = nullptr;
    QOpenGLTexture* texture_u_    = nullptr;
    QOpenGLTexture* texture_v_    = nullptr;
    QOpenGLTexture* texture_uv_   = nullptr;
    QOpenGLTexture* texture_rgba_ = nullptr;

    PlanePbo pbo_y_;
    PlanePbo pbo_u_;
    PlanePbo pbo_v_;
    PlanePbo pbo_uv_;
    PlanePbo pbo_rgba_;

    int              texture_width_  = 0;
    int              texture_height_ = 0;
    FramePixelFormat texture_format_ = FramePixelFormat::YUV420P;

    GLint yuv420p_uniforms_[3] = { -1, -1, -1 };
    GLint nv12_uniforms_[2]    = { -1, -1 };
    GLint rgba_uniform_tex_    = -1;
    GLint rgba_uniform_layout_ = -1;

    ScaleMode         keep_scale_mode_           = ScaleMode::KeepAspectRatio;
    int               scale_w_                   = 0;
    int               scale_h_                   = 0;
    FrameUpdatePolicy frame_update_policy_       = FrameUpdatePolicy::CoalesceLatest;
    int               last_vertex_video_width_   = 0;
    int               last_vertex_video_height_  = 0;
    int               last_vertex_widget_width_  = 0;
    int               last_vertex_widget_height_ = 0;
    int               last_vertex_scale_w_       = 0;
    int               last_vertex_scale_h_       = 0;

    bool use_pbo_upload_ = false;

    mutable std::mutex                 frame_mutex_;
    std::shared_ptr<const FrameBuffer> pending_frame_;
    std::shared_ptr<const FrameBuffer> current_frame_;
    std::atomic<bool>                  gl_initialized_{ false };
    std::atomic<bool>                  first_frame_notified_{ false };
    std::atomic<bool>                  update_scheduled_{ false };
    std::atomic<int>                   dropped_frames_{ 0 };

    std::atomic<bool> show_rec_indicator_{ false };
    QString           overlay_message_;
    XOverlayStyle     overlay_style_;

    FirstFrameCallback first_frame_cb_;

    int           render_fps_  = 0;
    int           frame_count_ = 0;
    QElapsedTimer fps_timer_;
    bool          fps_started_ = false;
};
