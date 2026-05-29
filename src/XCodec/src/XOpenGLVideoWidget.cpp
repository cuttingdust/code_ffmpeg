#include "XOpenGLVideoWidget.h"
#include "XOverlayUtil.h"
#include "AVConst.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include <QtGui/QPainter>
#include <QtGui/QPen>
#include <QtOpenGL/QOpenGLShader>
#include <QtOpenGL/QOpenGLShaderProgram>

namespace
{
    constexpr int kVertexAttrLocation   = 3;
    constexpr int kTexCoordAttrLocation = 4;
    constexpr int kPboThresholdPixels   = 1280 * 720;

    const char* kVertexShader = R"(
attribute vec4 vertexIn;
attribute vec2 textureIn;
varying vec2 textureOut;
void main(void)
{
    gl_Position = vertexIn;
    textureOut  = textureIn;
}
)";

    const char* kFragmentShaderYuv420p = R"(
varying vec2 textureOut;
uniform sampler2D tex_y;
uniform sampler2D tex_u;
uniform sampler2D tex_v;
void main(void)
{
    vec3 yuv;
    vec3 rgb;
    yuv.x        = texture2D(tex_y, textureOut).r;
    yuv.y        = texture2D(tex_u, textureOut).r - 0.5;
    yuv.z        = texture2D(tex_v, textureOut).r - 0.5;
    rgb          = mat3(1.0, 1.0, 1.0, 0.0, -0.39465, 2.03211, 1.13983, -0.58060, 0.0) * yuv;
    gl_FragColor = vec4(rgb, 1.0);
}
)";

    const char* kFragmentShaderNv12 = R"(
varying vec2 textureOut;
uniform sampler2D tex_y;
uniform sampler2D tex_uv;
void main(void)
{
    vec3 yuv;
    vec3 rgb;
    yuv.x        = texture2D(tex_y, textureOut).r;
    vec2 uv      = texture2D(tex_uv, textureOut).rg;
    yuv.y        = uv.r - 0.5;
    yuv.z        = uv.g - 0.5;
    rgb          = mat3(1.0, 1.0, 1.0, 0.0, -0.39465, 2.03211, 1.13983, -0.58060, 0.0) * yuv;
    gl_FragColor = vec4(rgb, 1.0);
}
)";

    const char* kFragmentShaderRgba = R"(
varying vec2 textureOut;
uniform sampler2D tex_rgba;
uniform int pixel_layout;
void main(void)
{
    vec4 color = texture2D(tex_rgba, textureOut);
    if (pixel_layout == 1)
    {
        color = vec4(color.b, color.g, color.r, color.a);
    }
    else if (pixel_layout == 2)
    {
        color = vec4(color.g, color.b, color.a, color.r);
    }
    gl_FragColor = color;
}
)";

    const GLfloat kVerticesFull[] = { -1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f };
    const GLfloat kTexCoords[]    = { 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f };
} // namespace

XOpenGLVideoWidget::XOpenGLVideoWidget(QWidget* parent) : QOpenGLWidget(parent)
{
    setUpdateBehavior(QOpenGLWidget::NoPartialUpdate);
    overlay_style_ = defaultRecOverlayStyle();
}

XOpenGLVideoWidget::~XOpenGLVideoWidget()
{
    makeCurrent();
    destroyPbos();
    destroyTextures();
    doneCurrent();
}

void XOpenGLVideoWidget::setScaleMode(ScaleMode mode)
{
    keep_scale_mode_           = mode;
    last_vertex_video_width_   = 0;
    last_vertex_video_height_  = 0;
    last_vertex_widget_width_  = 0;
    last_vertex_widget_height_ = 0;
    last_vertex_scale_w_       = 0;
    last_vertex_scale_h_       = 0;
    update();
}

XOpenGLVideoWidget::ScaleMode XOpenGLVideoWidget::scaleMode() const
{
    return keep_scale_mode_;
}

void XOpenGLVideoWidget::scale(int w, int h)
{
    scale_w_             = w;
    scale_h_             = h;
    last_vertex_scale_w_ = 0;
    last_vertex_scale_h_ = 0;
    update();
}

void XOpenGLVideoWidget::setScaleWidth(int w)
{
    scale_w_             = w;
    last_vertex_scale_w_ = 0;
    update();
}

void XOpenGLVideoWidget::setScaleHeight(int h)
{
    scale_h_             = h;
    last_vertex_scale_h_ = 0;
    update();
}

int XOpenGLVideoWidget::scaleWidth() const
{
    return scale_w_;
}

int XOpenGLVideoWidget::scaleHeight() const
{
    return scale_h_;
}

void XOpenGLVideoWidget::setFrameUpdatePolicy(FrameUpdatePolicy policy)
{
    frame_update_policy_ = policy;
}

XOpenGLVideoWidget::FrameUpdatePolicy XOpenGLVideoWidget::frameUpdatePolicy() const
{
    return frame_update_policy_;
}

void XOpenGLVideoWidget::setRecordingIndicator(bool show)
{
    show_rec_indicator_ = show;
    QMetaObject::invokeMethod(this, [this]() { update(); }, Qt::QueuedConnection);
}

bool XOpenGLVideoWidget::recordingIndicator() const
{
    return show_rec_indicator_.load();
}

void XOpenGLVideoWidget::setOverlayMessage(const QString& message)
{
    overlay_message_ = message;
    QMetaObject::invokeMethod(this, [this]() { update(); }, Qt::QueuedConnection);
}

QString XOpenGLVideoWidget::overlayMessage() const
{
    return overlay_message_;
}

void XOpenGLVideoWidget::setOverlayStyle(const XOverlayStyle& style)
{
    overlay_style_ = style;
    update();
}

XOverlayStyle XOpenGLVideoWidget::overlayStyle() const
{
    return overlay_style_;
}

int XOpenGLVideoWidget::videoWidth() const
{
    std::scoped_lock lock(frame_mutex_);
    return current_frame_ ? current_frame_->width : 0;
}

int XOpenGLVideoWidget::videoHeight() const
{
    std::scoped_lock lock(frame_mutex_);
    return current_frame_ ? current_frame_->height : 0;
}

int XOpenGLVideoWidget::renderFps() const
{
    return render_fps_;
}

int XOpenGLVideoWidget::droppedFrames() const
{
    return dropped_frames_.load();
}

void XOpenGLVideoWidget::submitFrame(const AVFrame* frame)
{
    if (!frame || !frame->data[0])
    {
        return;
    }

    if (frame_update_policy_ == FrameUpdatePolicy::DropWhenBusy && update_scheduled_.load(std::memory_order_acquire))
    {
        dropped_frames_.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    auto buffer = std::make_shared<FrameBuffer>();
    if (frame->format == AV_PIX_FMT_YUV420P)
    {
        copyYuv420p(frame, *buffer);
    }
    else if (frame->format == AV_PIX_FMT_NV12)
    {
        copyNv12(frame, *buffer);
    }
    else if (frame->format == AV_PIX_FMT_RGBA)
    {
        copyRgba(frame, *buffer);
    }
    else if (frame->format == AV_PIX_FMT_BGRA)
    {
        copyBgra(frame, *buffer);
    }
    else if (frame->format == AV_PIX_FMT_ARGB)
    {
        copyArgb(frame, *buffer);
    }
    else if (frame->format == AV_PIX_FMT_RGB24)
    {
        copyRgb24(frame, *buffer);
    }
    else if (frame->format == AV_PIX_FMT_BGR24)
    {
        copyBgr24(frame, *buffer);
    }
    else if (!convertToYuv420p(frame, *buffer))
    {
        return;
    }

    {
        std::scoped_lock lock(frame_mutex_);
        pending_frame_ = std::move(buffer);
    }

    if (frame_update_policy_ == FrameUpdatePolicy::QueueEveryFrame)
    {
        QMetaObject::invokeMethod(this, [this]() { update(); }, Qt::QueuedConnection);
        return;
    }

    if (update_scheduled_.exchange(true))
    {
        dropped_frames_.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    QMetaObject::invokeMethod(
            this,
            [this]()
            {
                update_scheduled_ = false;
                update();
            },
            Qt::QueuedConnection);
}

void XOpenGLVideoWidget::initializeGL()
{
    initializeOpenGLFunctions();
    initShaders();
    initGeometry();
}

void XOpenGLVideoWidget::resizeGL(int w, int h)
{
    Q_UNUSED(w)
    Q_UNUSED(h)
    glViewport(0, 0, width(), height());
    last_vertex_widget_width_  = 0;
    last_vertex_widget_height_ = 0;
    last_vertex_scale_w_       = 0;
    last_vertex_scale_h_       = 0;
}

void XOpenGLVideoWidget::paintGL()
{
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    std::shared_ptr<const FrameBuffer> frame;
    {
        std::scoped_lock lock(frame_mutex_);
        if (pending_frame_)
        {
            current_frame_ = pending_frame_;
            pending_frame_.reset();
        }
        frame = current_frame_;
    }

    if (frame && (!frame->y.empty() || !frame->rgba.empty()))
    {
        use_pbo_upload_ = frame->width * frame->height >= kPboThresholdPixels;
        ensureTextures(frame->width, frame->height, frame->format);
        updateVideoVertices(frame->width, frame->height);
        updateRenderFps();

        switch (frame->format)
        {
            case FramePixelFormat::NV12:
                drawNv12Frame(*frame);
                break;
            case FramePixelFormat::RGBA:
            case FramePixelFormat::BGRA:
            case FramePixelFormat::ARGB:
                drawRgbaFrame(*frame);
                break;
            default:
                drawYuv420pFrame(*frame);
                break;
        }
    }

    drawOverlays();
}

void XOpenGLVideoWidget::initShaders()
{
    auto linkProgram = [](QOpenGLShaderProgram& program, const char* fragment_shader)
    {
        program.addShaderFromSourceCode(QOpenGLShader::Vertex, kVertexShader);
        program.addShaderFromSourceCode(QOpenGLShader::Fragment, fragment_shader);
        program.bindAttributeLocation("vertexIn", kVertexAttrLocation);
        program.bindAttributeLocation("textureIn", kTexCoordAttrLocation);
        program.link();
    };

    linkProgram(program_yuv420p_, kFragmentShaderYuv420p);
    linkProgram(program_nv12_, kFragmentShaderNv12);
    linkProgram(program_rgba_, kFragmentShaderRgba);

    yuv420p_uniforms_[0] = program_yuv420p_.uniformLocation("tex_y");
    yuv420p_uniforms_[1] = program_yuv420p_.uniformLocation("tex_u");
    yuv420p_uniforms_[2] = program_yuv420p_.uniformLocation("tex_v");

    nv12_uniforms_[0] = program_nv12_.uniformLocation("tex_y");
    nv12_uniforms_[1] = program_nv12_.uniformLocation("tex_uv");

    rgba_uniform_tex_    = program_rgba_.uniformLocation("tex_rgba");
    rgba_uniform_layout_ = program_rgba_.uniformLocation("pixel_layout");
}

void XOpenGLVideoWidget::initGeometry()
{
    vao_.create();
    vao_.bind();

    vbo_vertices_.create();
    vbo_vertices_.bind();
    vbo_vertices_.allocate(kVerticesFull, sizeof(kVerticesFull));

    vbo_texcoords_.create();
    vbo_texcoords_.bind();
    vbo_texcoords_.allocate(kTexCoords, sizeof(kTexCoords));

    vao_.release();
}

void XOpenGLVideoWidget::bindGeometry(QOpenGLShaderProgram& program)
{
    vao_.bind();

    vbo_vertices_.bind();
    program.setAttributeBuffer(kVertexAttrLocation, GL_FLOAT, 0, 2, 0);
    program.enableAttributeArray(kVertexAttrLocation);
    vbo_vertices_.release();

    vbo_texcoords_.bind();
    program.setAttributeBuffer(kTexCoordAttrLocation, GL_FLOAT, 0, 2, 0);
    program.enableAttributeArray(kTexCoordAttrLocation);
    vbo_texcoords_.release();
}

void XOpenGLVideoWidget::updateVideoVertices(int video_width, int video_height)
{
    const int widget_width  = std::max(width(), 1);
    const int widget_height = std::max(height(), 1);
    const int target_width  = scale_w_ > 0 ? scale_w_ : widget_width;
    const int target_height = scale_h_ > 0 ? scale_h_ : widget_height;

    if (last_vertex_video_width_ == video_width && last_vertex_video_height_ == video_height &&
        last_vertex_widget_width_ == widget_width && last_vertex_widget_height_ == widget_height &&
        last_vertex_scale_w_ == target_width && last_vertex_scale_h_ == target_height)
    {
        return;
    }

    last_vertex_video_width_   = video_width;
    last_vertex_video_height_  = video_height;
    last_vertex_widget_width_  = widget_width;
    last_vertex_widget_height_ = widget_height;
    last_vertex_scale_w_       = target_width;
    last_vertex_scale_h_       = target_height;

    const auto to_ndc_x = [widget_width](float px) { return -1.0f + 2.0f * px / static_cast<float>(widget_width); };
    const auto to_ndc_y = [widget_height](float py) { return 1.0f - 2.0f * py / static_cast<float>(widget_height); };

    float px0 = 0.0f;
    float py0 = 0.0f;
    float px1 = static_cast<float>(target_width);
    float py1 = static_cast<float>(target_height);

    if (keep_scale_mode_ == ScaleMode::KeepAspectRatio && video_width > 0 && video_height > 0)
    {
        const float video_aspect  = static_cast<float>(video_width) / static_cast<float>(video_height);
        const float target_aspect = static_cast<float>(target_width) / static_cast<float>(target_height);
        float       draw_w        = static_cast<float>(target_width);
        float       draw_h        = static_cast<float>(target_height);
        float       offset_x      = 0.0f;
        float       offset_y      = 0.0f;

        if (video_aspect > target_aspect)
        {
            draw_h   = static_cast<float>(target_width) / video_aspect;
            offset_y = (static_cast<float>(target_height) - draw_h) * 0.5f;
        }
        else
        {
            draw_w   = static_cast<float>(target_height) * video_aspect;
            offset_x = (static_cast<float>(target_width) - draw_w) * 0.5f;
        }

        px0 = offset_x;
        py0 = offset_y;
        px1 = offset_x + draw_w;
        py1 = offset_y + draw_h;
    }

    const float x0 = to_ndc_x(px0);
    const float x1 = to_ndc_x(px1);
    const float y0 = to_ndc_y(py1);
    const float y1 = to_ndc_y(py0);

    const GLfloat vertices[8] = { x0, y0, x1, y0, x0, y1, x1, y1 };

    vbo_vertices_.bind();
    vbo_vertices_.write(0, vertices, sizeof(vertices));
    vbo_vertices_.release();
}

void XOpenGLVideoWidget::ensureTextures(int width, int height, FramePixelFormat format)
{
    if (width <= 0 || height <= 0)
    {
        return;
    }

    if (texture_y_ && texture_width_ == width && texture_height_ == height && texture_format_ == format)
    {
        return;
    }

    destroyPbos();
    destroyTextures();

    texture_width_  = width;
    texture_height_ = height;
    texture_format_ = format;

    texture_y_ = new QOpenGLTexture(QOpenGLTexture::Target2D);
    texture_y_->setSize(width, height);
    texture_y_->setFormat(QOpenGLTexture::R8_UNorm);
    texture_y_->setWrapMode(QOpenGLTexture::ClampToEdge);
    texture_y_->setMinificationFilter(QOpenGLTexture::Linear);
    texture_y_->setMagnificationFilter(QOpenGLTexture::Linear);
    texture_y_->allocateStorage();

    if (format == FramePixelFormat::NV12)
    {
        texture_uv_ = new QOpenGLTexture(QOpenGLTexture::Target2D);
        texture_uv_->setSize(width / 2, height / 2);
        texture_uv_->setFormat(QOpenGLTexture::RG8_UNorm);
        texture_uv_->setWrapMode(QOpenGLTexture::ClampToEdge);
        texture_uv_->setMinificationFilter(QOpenGLTexture::Linear);
        texture_uv_->setMagnificationFilter(QOpenGLTexture::Linear);
        texture_uv_->allocateStorage();
    }
    else if (format == FramePixelFormat::RGBA || format == FramePixelFormat::BGRA || format == FramePixelFormat::ARGB)
    {
        texture_rgba_ = new QOpenGLTexture(QOpenGLTexture::Target2D);
        texture_rgba_->setSize(width, height);
        texture_rgba_->setFormat(QOpenGLTexture::RGBA8_UNorm);
        texture_rgba_->setWrapMode(QOpenGLTexture::ClampToEdge);
        texture_rgba_->setMinificationFilter(QOpenGLTexture::Linear);
        texture_rgba_->setMagnificationFilter(QOpenGLTexture::Linear);
        texture_rgba_->allocateStorage();
    }
    else
    {
        texture_u_ = new QOpenGLTexture(QOpenGLTexture::Target2D);
        texture_u_->setSize(width / 2, height / 2);
        texture_u_->setFormat(QOpenGLTexture::R8_UNorm);
        texture_u_->setWrapMode(QOpenGLTexture::ClampToEdge);
        texture_u_->setMinificationFilter(QOpenGLTexture::Linear);
        texture_u_->setMagnificationFilter(QOpenGLTexture::Linear);
        texture_u_->allocateStorage();

        texture_v_ = new QOpenGLTexture(QOpenGLTexture::Target2D);
        texture_v_->setSize(width / 2, height / 2);
        texture_v_->setFormat(QOpenGLTexture::R8_UNorm);
        texture_v_->setWrapMode(QOpenGLTexture::ClampToEdge);
        texture_v_->setMinificationFilter(QOpenGLTexture::Linear);
        texture_v_->setMagnificationFilter(QOpenGLTexture::Linear);
        texture_v_->allocateStorage();
    }
}

void XOpenGLVideoWidget::destroyTextures()
{
    delete texture_y_;
    delete texture_u_;
    delete texture_v_;
    delete texture_uv_;
    delete texture_rgba_;
    texture_y_      = nullptr;
    texture_u_      = nullptr;
    texture_v_      = nullptr;
    texture_uv_     = nullptr;
    texture_rgba_   = nullptr;
    texture_width_  = 0;
    texture_height_ = 0;
}

void XOpenGLVideoWidget::destroyPbos()
{
    pbo_y_.destroy();
    pbo_u_.destroy();
    pbo_v_.destroy();
    pbo_uv_.destroy();
    pbo_rgba_.destroy();
}

void XOpenGLVideoWidget::uploadTextureData(QOpenGLTexture* texture, const uint8_t* data, int width, int height,
                                           QOpenGLTexture::PixelFormat pixel_format, PlanePbo& pbo)
{
    if (!texture || !data || width <= 0 || height <= 0)
    {
        return;
    }

    const int bytes_per_pixel = (pixel_format == QOpenGLTexture::RG) ? 2
            : (pixel_format == QOpenGLTexture::RGBA)                 ? 4
                                                                     : 1;
    const int size            = width * height * bytes_per_pixel;

    if (!use_pbo_upload_)
    {
        texture->bind();
        texture->setData(pixel_format, QOpenGLTexture::UInt8, data);
        return;
    }

    QOpenGLBuffer& pbo_buffer = pbo.acquire();
    if (!pbo_buffer.isCreated())
    {
        pbo_buffer.create();
    }

    pbo_buffer.bind();
    if (pbo.capacity < size)
    {
        pbo_buffer.allocate(size);
        pbo.capacity = size;
    }

    if (void* mapped = pbo_buffer.map(QOpenGLBuffer::WriteOnly))
    {
        std::memcpy(mapped, data, static_cast<size_t>(size));
        pbo_buffer.unmap();
    }

    texture->bind();
    const GLenum gl_format = (pixel_format == QOpenGLTexture::RG) ? GL_RG
            : (pixel_format == QOpenGLTexture::RGBA)              ? GL_RGBA
                                                                  : GL_RED;
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, gl_format, GL_UNSIGNED_BYTE, nullptr);
    pbo_buffer.release();
}

void XOpenGLVideoWidget::drawYuv420pFrame(const FrameBuffer& frame)
{
    if (!texture_y_ || !texture_u_ || !texture_v_)
    {
        return;
    }

    uploadTextureData(texture_y_, frame.y.data(), frame.width, frame.height, QOpenGLTexture::Red, pbo_y_);
    uploadTextureData(texture_u_, frame.u.data(), frame.width / 2, frame.height / 2, QOpenGLTexture::Red, pbo_u_);
    uploadTextureData(texture_v_, frame.v.data(), frame.width / 2, frame.height / 2, QOpenGLTexture::Red, pbo_v_);

    program_yuv420p_.bind();
    bindGeometry(program_yuv420p_);

    texture_y_->bind(0);
    program_yuv420p_.setUniformValue(yuv420p_uniforms_[0], 0);
    texture_u_->bind(1);
    program_yuv420p_.setUniformValue(yuv420p_uniforms_[1], 1);
    texture_v_->bind(2);
    program_yuv420p_.setUniformValue(yuv420p_uniforms_[2], 2);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    vao_.release();
    program_yuv420p_.release();
}

void XOpenGLVideoWidget::drawNv12Frame(const FrameBuffer& frame)
{
    if (!texture_y_ || !texture_uv_)
    {
        return;
    }

    uploadTextureData(texture_y_, frame.y.data(), frame.width, frame.height, QOpenGLTexture::Red, pbo_y_);
    uploadTextureData(texture_uv_, frame.uv.data(), frame.width / 2, frame.height / 2, QOpenGLTexture::RG, pbo_uv_);

    program_nv12_.bind();
    bindGeometry(program_nv12_);

    texture_y_->bind(0);
    program_nv12_.setUniformValue(nv12_uniforms_[0], 0);
    texture_uv_->bind(1);
    program_nv12_.setUniformValue(nv12_uniforms_[1], 1);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    vao_.release();
    program_nv12_.release();
}

void XOpenGLVideoWidget::drawRgbaFrame(const FrameBuffer& frame)
{
    if (!texture_rgba_ || frame.rgba.empty())
    {
        return;
    }

    uploadTextureData(texture_rgba_, frame.rgba.data(), frame.width, frame.height, QOpenGLTexture::RGBA, pbo_rgba_);

    program_rgba_.bind();
    bindGeometry(program_rgba_);

    texture_rgba_->bind(0);
    program_rgba_.setUniformValue(rgba_uniform_tex_, 0);
    int pixel_layout = 0;
    if (frame.format == FramePixelFormat::BGRA)
    {
        pixel_layout = 1;
    }
    else if (frame.format == FramePixelFormat::ARGB)
    {
        pixel_layout = 2;
    }
    program_rgba_.setUniformValue(rgba_uniform_layout_, pixel_layout);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    vao_.release();
    program_rgba_.release();
}

void XOpenGLVideoWidget::drawOverlays()
{
    QPainter painter(this);

    if (!overlay_message_.isEmpty())
    {
        painter.setPen(Qt::white);
        painter.drawText(rect(), Qt::AlignCenter, overlay_message_);
    }

    if (show_rec_indicator_.load())
    {
        const auto& style        = overlay_style_;
        const int   dot_size     = style.dot_radius * 2;
        const int   total_width  = style.padding_left + dot_size + style.spacing + 42 + style.padding_right;
        const int   total_height = std::max(dot_size, style.font_size + 4) + style.padding_top + style.padding_bottom;

        QRect rec_rect(style.margin_left, style.margin_top, total_width, total_height);

        painter.save();
        if (style.show_border)
        {
            painter.setPen(QPen(QColor(style.border_r, style.border_g, style.border_b, style.border_a), 1));
        }
        else
        {
            painter.setPen(Qt::NoPen);
        }
        painter.setBrush(QColor(style.dot_r, style.dot_g, style.dot_b, 180));
        painter.drawRoundedRect(rec_rect, 4, 4);

        painter.setBrush(QColor(style.dot_r, style.dot_g, style.dot_b, style.dot_a));
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(QPoint(rec_rect.left() + style.padding_left + style.dot_radius,
                                   rec_rect.top() + style.padding_top + dot_size / 2),
                            style.dot_radius, style.dot_radius);

        painter.setPen(QColor(style.text_r, style.text_g, style.text_b, style.text_a));
        QFont font = painter.font();
        font.setPointSize(style.font_size);
        font.setBold(true);
        painter.setFont(font);
        painter.drawText(rec_rect.adjusted(style.padding_left + dot_size + style.spacing, 0, -style.padding_right, 0),
                         Qt::AlignVCenter | Qt::AlignLeft, "REC");
        painter.restore();
    }

    painter.end();
}

void XOpenGLVideoWidget::copyYuv420p(const AVFrame* frame, FrameBuffer& out)
{
    out.width  = frame->width;
    out.height = frame->height;
    out.format = FramePixelFormat::YUV420P;
    out.y.resize(static_cast<size_t>(out.width) * out.height);
    out.u.resize(static_cast<size_t>(out.width) * out.height / 4);
    out.v.resize(static_cast<size_t>(out.width) * out.height / 4);
    out.uv.clear();
    out.rgba.clear();

    for (int row = 0; row < out.height; ++row)
    {
        std::memcpy(out.y.data() + row * out.width, frame->data[0] + row * frame->linesize[0],
                    static_cast<size_t>(out.width));
    }

    const int chroma_height = out.height / 2;
    const int chroma_width  = out.width / 2;
    for (int row = 0; row < chroma_height; ++row)
    {
        std::memcpy(out.u.data() + row * chroma_width, frame->data[1] + row * frame->linesize[1],
                    static_cast<size_t>(chroma_width));
        std::memcpy(out.v.data() + row * chroma_width, frame->data[2] + row * frame->linesize[2],
                    static_cast<size_t>(chroma_width));
    }
}

void XOpenGLVideoWidget::copyNv12(const AVFrame* frame, FrameBuffer& out)
{
    out.width  = frame->width;
    out.height = frame->height;
    out.format = FramePixelFormat::NV12;
    out.y.resize(static_cast<size_t>(out.width) * out.height);
    out.uv.resize(static_cast<size_t>(out.width) * out.height / 2);
    out.u.clear();
    out.v.clear();
    out.rgba.clear();

    for (int row = 0; row < out.height; ++row)
    {
        std::memcpy(out.y.data() + row * out.width, frame->data[0] + row * frame->linesize[0],
                    static_cast<size_t>(out.width));
    }

    const int chroma_height = out.height / 2;
    for (int row = 0; row < chroma_height; ++row)
    {
        std::memcpy(out.uv.data() + row * out.width, frame->data[1] + row * frame->linesize[1],
                    static_cast<size_t>(out.width));
    }
}

void XOpenGLVideoWidget::copyRgba(const AVFrame* frame, FrameBuffer& out)
{
    out.width  = frame->width;
    out.height = frame->height;
    out.format = FramePixelFormat::RGBA;
    out.rgba.resize(static_cast<size_t>(out.width) * out.height * 4);
    out.y.clear();
    out.u.clear();
    out.v.clear();
    out.uv.clear();

    for (int row = 0; row < out.height; ++row)
    {
        std::memcpy(out.rgba.data() + static_cast<size_t>(row) * out.width * 4,
                    frame->data[0] + row * frame->linesize[0], static_cast<size_t>(out.width) * 4);
    }
}

void XOpenGLVideoWidget::copyBgra(const AVFrame* frame, FrameBuffer& out)
{
    out.width  = frame->width;
    out.height = frame->height;
    out.format = FramePixelFormat::BGRA;
    out.rgba.resize(static_cast<size_t>(out.width) * out.height * 4);
    out.y.clear();
    out.u.clear();
    out.v.clear();
    out.uv.clear();

    for (int row = 0; row < out.height; ++row)
    {
        std::memcpy(out.rgba.data() + static_cast<size_t>(row) * out.width * 4,
                    frame->data[0] + row * frame->linesize[0], static_cast<size_t>(out.width) * 4);
    }
}

void XOpenGLVideoWidget::copyArgb(const AVFrame* frame, FrameBuffer& out)
{
    out.width  = frame->width;
    out.height = frame->height;
    out.format = FramePixelFormat::ARGB;
    out.rgba.resize(static_cast<size_t>(out.width) * out.height * 4);
    out.y.clear();
    out.u.clear();
    out.v.clear();
    out.uv.clear();

    for (int row = 0; row < out.height; ++row)
    {
        std::memcpy(out.rgba.data() + static_cast<size_t>(row) * out.width * 4,
                    frame->data[0] + row * frame->linesize[0], static_cast<size_t>(out.width) * 4);
    }
}

void XOpenGLVideoWidget::copyRgb24(const AVFrame* frame, FrameBuffer& out)
{
    out.width  = frame->width;
    out.height = frame->height;
    out.format = FramePixelFormat::RGBA;
    out.rgba.resize(static_cast<size_t>(out.width) * out.height * 4);
    out.y.clear();
    out.u.clear();
    out.v.clear();
    out.uv.clear();

    for (int row = 0; row < out.height; ++row)
    {
        const uint8_t* src = frame->data[0] + row * frame->linesize[0];
        uint8_t*       dst = out.rgba.data() + static_cast<size_t>(row) * out.width * 4;
        for (int col = 0; col < out.width; ++col)
        {
            dst[col * 4 + 0] = src[col * 3 + 0];
            dst[col * 4 + 1] = src[col * 3 + 1];
            dst[col * 4 + 2] = src[col * 3 + 2];
            dst[col * 4 + 3] = 255;
        }
    }
}

void XOpenGLVideoWidget::copyBgr24(const AVFrame* frame, FrameBuffer& out)
{
    out.width  = frame->width;
    out.height = frame->height;
    out.format = FramePixelFormat::RGBA;
    out.rgba.resize(static_cast<size_t>(out.width) * out.height * 4);
    out.y.clear();
    out.u.clear();
    out.v.clear();
    out.uv.clear();

    for (int row = 0; row < out.height; ++row)
    {
        const uint8_t* src = frame->data[0] + row * frame->linesize[0];
        uint8_t*       dst = out.rgba.data() + static_cast<size_t>(row) * out.width * 4;
        for (int col = 0; col < out.width; ++col)
        {
            dst[col * 4 + 0] = src[col * 3 + 2];
            dst[col * 4 + 1] = src[col * 3 + 1];
            dst[col * 4 + 2] = src[col * 3 + 0];
            dst[col * 4 + 3] = 255;
        }
    }
}

bool XOpenGLVideoWidget::convertToYuv420p(const AVFrame* frame, FrameBuffer& out)
{
    out.width  = frame->width;
    out.height = frame->height;
    out.format = FramePixelFormat::YUV420P;
    out.y.resize(static_cast<size_t>(out.width) * out.height);
    out.u.resize(static_cast<size_t>(out.width) * out.height / 4);
    out.v.resize(static_cast<size_t>(out.width) * out.height / 4);
    out.uv.clear();
    out.rgba.clear();

    uint8_t* dst_data[4]     = { out.y.data(), out.u.data(), out.v.data(), nullptr };
    int      dst_linesize[4] = { out.width, out.width / 2, out.width / 2, 0 };

    SwsContext* sws = sws_getContext(frame->width, frame->height, static_cast<AVPixelFormat>(frame->format), out.width,
                                     out.height, AV_PIX_FMT_YUV420P, SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!sws)
    {
        return false;
    }

    sws_scale(sws, frame->data, frame->linesize, 0, frame->height, dst_data, dst_linesize);
    sws_freeContext(sws);
    return true;
}

void XOpenGLVideoWidget::updateRenderFps()
{
    ++frame_count_;
    if (!fps_started_)
    {
        fps_timer_.start();
        fps_started_ = true;
        return;
    }

    if (fps_timer_.elapsed() >= 1000)
    {
        render_fps_  = frame_count_;
        frame_count_ = 0;
        fps_timer_.restart();
    }
}
