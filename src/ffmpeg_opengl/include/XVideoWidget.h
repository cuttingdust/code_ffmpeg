#pragma once


#include <QtCore/QTimer>
#include <QtGui/QOpenGLFunctions>
#include <QtOpenGLWidgets/QOpenGLWidget>
#include <QtOpenGL/QOpenGLShaderProgram>
#include <QtOpenGL/QOpenGLVertexArrayObject>
#include <QtOpenGL/QOpenGLBuffer>
#include <QtOpenGL/QOpenGLTexture>

class XVideoWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT
public:
    explicit XVideoWidget(QWidget *parent = Q_NULLPTR);
    ~XVideoWidget() override;

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

private:
    QOpenGLShaderProgram program_;

    /// Qt 封装的 OpenGL 对象
    QOpenGLVertexArrayObject vao_;          ///< VAO
    QOpenGLBuffer            vboVertices_;  ///< VAO
    QOpenGLBuffer            vboTexCoords_; ///< VAO
    QOpenGLBuffer            vbo_;          ///< VBO（存储顶点和材质坐标）


    /// shader中yuv变量地址
    GLuint unis[3] = { 0 };
    /// opengl的 texture地址
    GLuint texs[3] = { 0 };

    int width_  = 400;
    int height_ = 300;

    QOpenGLTexture *textureY_ = nullptr;
    QOpenGLTexture *textureU_ = nullptr;
    QOpenGLTexture *textureV_ = nullptr;

    /// 材质内存空间
    unsigned char *datas_[3] = { 0 };

    FILE *fp_ = NULL;
};
