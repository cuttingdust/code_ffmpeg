#include "SdlQtRGB.h"

#include "ui_sdlqtrgb.h"
#include "XVideoView.h"

#include <SDL2/SDL.h>

extern "C" {
#include <libavcodec/avcodec.h>
}

#include <QtGui/QImage>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QSpinBox>

#include <fstream>
#include <iostream>
#include <algorithm>

extern void MSleep(unsigned int ms);

static int sdl_width  = 0;
static int sdl_height = 0;

static std::ifstream yuv_file;

static XVideoView *view  = nullptr;
static AVFrame    *frame = nullptr;

static long long file_size = 0;
static QLabel   *view_fps  = nullptr;
static QSpinBox *set_fps   = nullptr; /// 设置fps控件
int              fps       = 25;      /// 播放帧率

SdlQtRGB::SdlQtRGB(QWidget *parent) : QWidget(parent)
{
    /// 打开yuv文件
    yuv_file.open(R"(.\assert\out_400_300_25.yuv)", std::ios::binary);
    if (!yuv_file)
    {
        QMessageBox::information(this, "", "open yuv failed!");
        return;
    }
    yuv_file.seekg(0, std::ios::end); /// 移到文件结尾
    file_size = yuv_file.tellg();     /// 文件指针位置
    yuv_file.seekg(0, std::ios::beg);

    ui_ = new Ui::SdlQtRGBClass;
    ui_->setupUi(this);

    /// 绑定渲染信号槽
    connect(this, &SdlQtRGB::signalView, this, &SdlQtRGB::slotView);

    /// 显示fps的控件
    view_fps = new QLabel(this);
    view_fps->setText("fps:100");

    /// 设置fps
    set_fps = new QSpinBox(this);
    set_fps->move(200, 0);
    set_fps->setValue(25);
    set_fps->setRange(1, 300);


    sdl_width  = 400;
    sdl_height = 300;
    ui_->label->resize(sdl_width, sdl_height);
    view = XVideoView::create();
    // view->init(sdl_width, sdl_height, XVideoView::YUV420P);
    // view->close();
    view->init(sdl_width, sdl_height, XVideoView::YUV420P, (void *)ui_->label->winId());

    /// 生成frame对象空间
    frame         = av_frame_alloc();
    frame->width  = sdl_width;
    frame->height = sdl_height;
    frame->format = AV_PIX_FMT_YUV420P;
    //////////////////////////////////////////////////////////////////
    ///  Y Y
    ///   UV
    ///  Y Y
    frame->linesize[0] = sdl_width;     /// Y
    frame->linesize[1] = sdl_width / 2; /// U
    frame->linesize[2] = sdl_width / 2; /// V
    /// 生成图像空间 默认32字节对齐
    auto re = av_frame_get_buffer(frame, 0);
    if (re != 0)
    {
        char buf[1024] = { 0 };
        av_strerror(re, buf, sizeof(buf));
        std::cerr << buf << std::endl;
    }

    // startTimer(10);

    th_ = std::thread(&SdlQtRGB::threadFunc, this);
}

SdlQtRGB::~SdlQtRGB()
{
    if (th_.joinable())
    {
        th_.join();
    }
    is_exit_ = true;
}

void SdlQtRGB::threadFunc()
{
    while (!is_exit_)
    {
        emit signalView();
        if (fps > 0)
        {
            MSleep(1000 / fps);
        }
        else
        {
            MSleep(10);
        }
    }
}

void SdlQtRGB::timerEvent(QTimerEvent *event)
{
    /// yuv_file.read((char*)yuv, sdl_width * sdl_height * 1.5);
    ///  yuv420p
    ///  4*2
    ///  yyyy yyyy
    ///  u    u
    ///  v    v

    yuv_file.read((char *)frame->data[0], sdl_width * sdl_height);     /// Y
    yuv_file.read((char *)frame->data[1], sdl_width * sdl_height / 4); /// U
    yuv_file.read((char *)frame->data[2], sdl_width * sdl_height / 4); /// V

    if (view->isExit())
    {
        view->close();
        exit(0);
    }
    view->drawFrame(frame);
}

void SdlQtRGB::resizeEvent(QResizeEvent *event)
{
    ui_->label->resize(size());
    ui_->label->move(0, 0);
    view->scale(width(), height());
}

void SdlQtRGB::slotView()
{
    yuv_file.read((char *)frame->data[0], sdl_width * sdl_height);     /// Y
    yuv_file.read((char *)frame->data[1], sdl_width * sdl_height / 4); /// U
    yuv_file.read((char *)frame->data[2], sdl_width * sdl_height / 4); /// V
    if (yuv_file.tellg() == file_size)                                 /// 读取到文件结尾
    {
        yuv_file.seekg(0, std::ios::beg);
    }

    if (view->isExit())
    {
        view->close();
        exit(0);
    }
    view->drawFrame(frame);

    std::stringstream ss;
    ss << "fps:" << view->renderFps();

    /// 只能在槽函数中调用
    view_fps->setText(ss.str().c_str());
    fps = set_fps->value(); /// 拿到播放帧率
}
