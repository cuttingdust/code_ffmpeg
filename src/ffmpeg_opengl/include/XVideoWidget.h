#pragma once

#include <QtOpenGLWidgets/QOpenGLWidget>
#include <QtGui/QOpenGLFunctions>
#include <QtOpenGL/QOpenGLShaderProgram>
#include <QtOpenGL/QOpenGLVertexArrayObject>
#include <QtOpenGL/QOpenGLBuffer>
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
};
