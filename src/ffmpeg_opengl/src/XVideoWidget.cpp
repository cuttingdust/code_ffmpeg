#include "XVideoWidget.h"

/// 自动加双引号
#define GET_STR(x) #x
#define A_VER      3
#define T_VER      4

/// 顶点shader
const char *vString = GET_STR(     ///
        attribute vec4 vertexIn;   ///
        attribute vec2 textureIn;  ///
        varying vec2   textureOut; ///
        void           main(void) {
            gl_Position = vertexIn;
            textureOut  = textureIn;
        });

/// 片元shader
const char *tString = GET_STR(                                                ///
        varying vec2      textureOut;                                         ///
        uniform sampler2D tex_y;                                              ///
        uniform sampler2D tex_u;                                              ///
        uniform sampler2D tex_v;                                              ///
        void              main(void) {                                        ///
            vec3 yuv;                                            ///
            vec3 rgb;                                            ///
            yuv.x        = texture2D(tex_y, textureOut).r;       ///
            yuv.y        = texture2D(tex_u, textureOut).r - 0.5; ///
            yuv.z        = texture2D(tex_v, textureOut).r - 0.5; ///
            rgb          = mat3(1.0, 1.0, 1.0, 0.0, -0.39465, 2.03211, 1.13983, -0.58060, 0.0) * yuv;
            gl_FragColor = vec4(rgb, 1.0);
        }

);


XVideoWidget::XVideoWidget(QWidget *parent) : QOpenGLWidget(parent)
{
}

XVideoWidget::~XVideoWidget() = default;

void XVideoWidget::initializeGL()
{
    qDebug() << "initializeGL";

    /// 初始化opengl （QOpenGLFunctions继承）函数
    initializeOpenGLFunctions();

    QOpenGLContext *ctx = QOpenGLContext::currentContext();
    if (ctx)
    {
        qDebug() << "OpenGL version:" << ctx->format().majorVersion() << "." << ctx->format().minorVersion();
    }

    /// program加载shader（顶点和片元）脚本
    /// 片元（像素）
    qDebug() << program_.addShaderFromSourceCode(QOpenGLShader::Fragment, tString);
    /// 顶点shader
    qDebug() << program_.addShaderFromSourceCode(QOpenGLShader::Vertex, vString);

    /// 设置顶点坐标的变量
    program_.bindAttributeLocation("vertexIn", A_VER);

    /// 设置材质坐标
    program_.bindAttributeLocation("textureIn", T_VER);

    /// 编译shader
    qDebug() << "program.link() = " << program_.link();

    qDebug() << "program.bind() = " << program_.bind();

    //传递顶点和材质坐标
    //顶点
    static const GLfloat ver[] = { -1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f };

    //材质
    static const GLfloat tex[] = { 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f };

    // //顶点
    // glVertexAttribPointer(A_VER, 2, GL_FLOAT, 0, 0, ver);
    // glEnableVertexAttribArray(A_VER);
    //
    // //材质
    // glVertexAttribPointer(T_VER, 2, GL_FLOAT, 0, 0, tex);
    // glEnableVertexAttribArray(T_VER);

    //////////////////////////////////////////////////////////////////

    /// 3. 创建并绑定 VAO
    vao_.create();
    vao_.bind();

    /// ========== 第一个 VBO：顶点坐标 ==========
    vboVertices_.create(); // 需要添加成员变量
    vboVertices_.bind();
    vboVertices_.allocate(ver, sizeof(ver));

    // 设置顶点属性（位置 0）
    program_.setAttributeBuffer(A_VER, GL_FLOAT, 0, 2, 0); // stride = 0（数据紧密排列）
    program_.enableAttributeArray(A_VER);

    vboVertices_.release(); // 解绑，但不是必须的

    // ========== 第二个 VBO：纹理坐标 ==========
    vboTexCoords_.create(); // 需要添加成员变量
    vboTexCoords_.bind();
    vboTexCoords_.allocate(tex, sizeof(tex));

    // 设置纹理属性（位置 1）
    program_.setAttributeBuffer(T_VER, GL_FLOAT, 0, 2, 0); // stride = 0
    program_.enableAttributeArray(T_VER);

    vboTexCoords_.release();

    // 解绑 VAO
    vao_.release();
    program_.release();

    //////////////////////////////////////////////////////////////////

    /// 你知道 这样存储方式 一种是结构体数组 就是SOA 还有一种 数组结构体 就是 AOS 对不对

    // /// 2. 准备顶点数据（顶点坐标 + 纹理坐标交错的数组）
    // /// 顶点坐标 (x, y) 和 纹理坐标 (u, v) 交错存储
    // struct Vertex
    // {
    //     float x, y; /// 顶点坐标
    //     float u, v; /// 纹理坐标
    // };
    //
    // Vertex vertices[] = {
    //     /// 第一个三角形
    //     { -1.0f, -1.0f, 0.0f, 1.0f }, ///< 左下
    //     { 1.0f, -1.0f, 1.0f, 1.0f },  ///< 右下
    //     { -1.0f, 1.0f, 0.0f, 0.0f },  ///< 左上
    //     /// 第二个三角形
    //     { 1.0f, -1.0f, 1.0f, 1.0f }, ///< 右下
    //     { 1.0f, 1.0f, 1.0f, 0.0f },  ///< 右上
    //     { -1.0f, 1.0f, 0.0f, 0.0f }  ///< 左上
    // };
    //
    // /// 3. 创建并绑定 VAO
    // vao_.create();
    // vao_.bind();
    //
    // /// 4. 创建并绑定 VBO，上传数据
    // vbo_.create();
    // vbo_.bind();
    // vbo_.allocate(vertices, sizeof(vertices));
    //
    //
    // /// 5. 设置顶点属性（位置 0）
    // program_.setAttributeBuffer(A_VER, GL_FLOAT, offsetof(Vertex, x), /// 偏移量 0
    //                             2,                                    /// 每个顶点有 2 个值 (x, y)
    //                             sizeof(Vertex));                      /// 步长 = 整个 Vertex 的大小
    // program_.enableAttributeArray(A_VER);
    //
    // /// 6. 设置纹理属性（位置 1）
    // program_.setAttributeBuffer(T_VER, GL_FLOAT, offsetof(Vertex, u), /// 偏移量 8 (两个 float 后)
    //                             2,                                    /// 每个纹理坐标有 2 个值 (u, v)
    //                             sizeof(Vertex));                      /// 步长 = 整个 Vertex 的大小
    // program_.enableAttributeArray(T_VER);
    //
    // /// 7. 解绑（不是必须的，但保持整洁）
    // vbo_.release();
    // vao_.release();
    //
    // /// 8. 释放 program（不是必须的，析构时会处理）
    // program_.release();
}

void XVideoWidget::resizeGL(int w, int h)
{
    qDebug() << "resizeGL" << w << h;
}

void XVideoWidget::paintGL()
{
    qDebug() << "paintGL";
}
