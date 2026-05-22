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
    vboVertices_.create();
    vboVertices_.bind();
    vboVertices_.allocate(ver, sizeof(ver));

    /// 设置顶点属性（位置 0）
    program_.setAttributeBuffer(A_VER, GL_FLOAT, 0, 2, 0); /// stride = 0（数据紧密排列）
    program_.enableAttributeArray(A_VER);

    vboVertices_.release(); /// 解绑，但不是必须的

    /// ========== 第二个 VBO：纹理坐标 ==========
    vboTexCoords_.create();
    vboTexCoords_.bind();
    vboTexCoords_.allocate(tex, sizeof(tex));

    /// 设置纹理属性（位置 1）
    program_.setAttributeBuffer(T_VER, GL_FLOAT, 0, 2, 0); /// stride = 0
    program_.enableAttributeArray(T_VER);

    vboTexCoords_.release();

    /// 解绑 VAO
    vao_.release();
    program_.release();

    //////////////////////////////////////////////////////////////////

    /// 存储方式 一种是结构体数组 就是SOA 还有一种 数组结构体 就是 AOS

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


    //////////////////////////////////////////////////////////////////

    /// 从shader获取材质
    unis[0] = program_.uniformLocation("tex_y");
    unis[1] = program_.uniformLocation("tex_u");
    unis[2] = program_.uniformLocation("tex_v");

    // /// 创建材质
    // glGenTextures(3, texs);

    // /// Y
    // glBindTexture(GL_TEXTURE_2D, texs[0]);
    // /// 放大过滤，线性插值
    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    // /// 创建材质显卡空间
    // glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width_, height_, 0, GL_RED, GL_UNSIGNED_BYTE, 0);

    textureY_ = new QOpenGLTexture(QOpenGLTexture::Target2D);
    textureY_->setSize(width_, height_);
    textureY_->setFormat(QOpenGLTexture::R8_UNorm); /// GL_RED, GL_UNSIGNED_BYTE
    textureY_->setWrapMode(QOpenGLTexture::ClampToEdge);
    textureY_->setMinificationFilter(QOpenGLTexture::Linear);
    textureY_->setMagnificationFilter(QOpenGLTexture::Linear);
    textureY_->allocateStorage(); /// 相当于 glTexImage2D(..., 0)


    /// U
    // glBindTexture(GL_TEXTURE_2D, texs[1]);
    // /// 放大过滤，线性插值
    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    // /// 创建材质显卡空间
    // glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width_ / 2, height_ / 2, 0, GL_RED, GL_UNSIGNED_BYTE, 0);

    textureU_ = new QOpenGLTexture(QOpenGLTexture::Target2D);
    textureU_->setSize(width_ / 2, height_ / 2);
    textureU_->setFormat(QOpenGLTexture::R8_UNorm);
    textureU_->setWrapMode(QOpenGLTexture::ClampToEdge);
    textureU_->setMinificationFilter(QOpenGLTexture::Linear);
    textureU_->setMagnificationFilter(QOpenGLTexture::Linear);
    textureU_->allocateStorage();

    /// V
    // glBindTexture(GL_TEXTURE_2D, texs[2]);
    // /// 放大过滤，线性插值
    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    // /// 创建材质显卡空间
    // glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width_ / 2, height_ / 2, 0, GL_RED, GL_UNSIGNED_BYTE, 0);

    textureV_ = new QOpenGLTexture(QOpenGLTexture::Target2D);
    textureV_->setSize(width_ / 2, height_ / 2);
    textureV_->setFormat(QOpenGLTexture::R8_UNorm);
    textureV_->setWrapMode(QOpenGLTexture::ClampToEdge);
    textureV_->setMinificationFilter(QOpenGLTexture::Linear);
    textureV_->setMagnificationFilter(QOpenGLTexture::Linear);
    textureV_->allocateStorage();


    ///分配材质内存空间
    datas_[0] = new unsigned char[width_ * height_];     /// Y
    datas_[1] = new unsigned char[width_ * height_ / 4]; /// U
    datas_[2] = new unsigned char[width_ * height_ / 4]; /// V


    auto filepath = R"(assert\400_300_25.yuv)";
    fp_           = fopen(filepath, "rb");
    if (!fp_)
    {
        qDebug() << filepath << "file open failed!";
    }


    /// 启动定时器
    QTimer *ti = new QTimer(this);
    connect(ti, SIGNAL(timeout()), this, SLOT(update()));
    ti->start(40);
}

void XVideoWidget::resizeGL(int w, int h)
{
    qDebug() << "resizeGL" << w << h;
}

void XVideoWidget::paintGL()
{
    qDebug() << "paintGL";

    if (feof(fp_))
    {
        fseek(fp_, 0, SEEK_SET);
    }
    fread(datas_[0], 1, width_ * height_, fp_);
    fread(datas_[1], 1, width_ * height_ / 4, fp_);
    fread(datas_[2], 1, width_ * height_ / 4, fp_);

    program_.bind();
    vao_.bind();

    // glActiveTexture(GL_TEXTURE0);
    // glBindTexture(GL_TEXTURE_2D, texs[0]); /// 0层绑定到Y材质
    // /// 修改材质内容(复制内存内容)
    // glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width_, height_, GL_RED, GL_UNSIGNED_BYTE, datas_[0]);
    // /// 与shader uni遍历关联
    // glUniform1i(unis[0], 0);

    //////////////////////////////////////////////////////////////////

    /// 激活纹理单元 0 并绑定 Y 纹理
    textureY_->bind(0); /// 自动执行 glActiveTexture(GL_TEXTURE0) + glBindTexture

    /// 更新纹理数据（相当于 glTexSubImage2D）
    // textureY_->setData(0, 0,                  /// x, y 偏移
    //                    QOpenGLTexture::Red,   /// 像素格式
    //                    QOpenGLTexture::UInt8, /// 数据类型
    //                    datas_[0]);            /// 数据指针
    ///
    textureY_->setData(QOpenGLTexture::Red, QOpenGLTexture::UInt8, datas_[0]);
    program_.setUniformValue(unis[0], 0);

    //////////////////////////////////////////////////////////////////

    // glActiveTexture(GL_TEXTURE0 + 1);
    // glBindTexture(GL_TEXTURE_2D, texs[1]); //1层绑定到U材质
    //                                        //修改材质内容(复制内存内容)
    // glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width / 2, height / 2, GL_RED, GL_UNSIGNED_BYTE, datas[1]);
    // //与shader uni遍历关联
    // glUniform1i(unis[1], 1);

    /// 激活纹理单元 0 并绑定 Y 纹理
    textureU_->bind(1); /// 自动执行 glActiveTexture(GL_TEXTURE0 + 1) + glBindTexture

    /// 更新纹理数据（相当于 glTexSubImage2D）
    // textureU_->setData(0, 0,                  /// x, y 偏移
    //                    QOpenGLTexture::Red,   /// 像素格式
    //                    QOpenGLTexture::UInt8, /// 数据类型
    //                    datas_[1]);            /// 数据指针
    ///
    textureU_->setData(QOpenGLTexture::Red, QOpenGLTexture::UInt8, datas_[1]);
    program_.setUniformValue(unis[1], 1);


    // glActiveTexture(GL_TEXTURE0 + 2);
    // glBindTexture(GL_TEXTURE_2D, texs[2]); //2层绑定到V材质
    //                                        //修改材质内容(复制内存内容)
    // glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width / 2, height / 2, GL_RED, GL_UNSIGNED_BYTE, datas[2]);
    // //与shader uni遍历关联
    // glUniform1i(unis[2], 2);

    /// 激活纹理单元 0 并绑定 Y 纹理
    textureV_->bind(2); /// 自动执行 glActiveTexture(GL_TEXTURE0 + 2) + glBindTexture

    /// 更新纹理数据（相当于 glTexSubImage2D）
    // textureV_->setData(0, 0,                  /// x, y 偏移
    //                    QOpenGLTexture::Red,   /// 像素格式
    //                    QOpenGLTexture::UInt8, /// 数据类型
    //                    datas_[2]);            /// 数据指针
    ///
    textureV_->setData(QOpenGLTexture::Red, QOpenGLTexture::UInt8, datas_[2]);
    program_.setUniformValue(unis[2], 2);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    qDebug() << "paintGL";

    vao_.release();
    program_.release();
}
