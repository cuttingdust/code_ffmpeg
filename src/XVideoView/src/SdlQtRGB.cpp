#include "SdlQtRGB.h"

#include "ui_sdlqtrgb.h"
#include "XVideoView.h"

#include <SDL2/SDL.h>
extern "C" {
#include <libavcodec/avcodec.h>
}

#include <QtGui/QImage>
#include <QtWidgets/QMessageBox>

#include <fstream>
#include <iostream>
#include <algorithm>

static int sdl_width  = 0;
static int sdl_height = 0;

// static unsigned char *yuv      = NULL;
// static int pix_size = 2;

static std::ifstream yuv_file;

static XVideoView *view  = nullptr;
static AVFrame    *frame = nullptr;

SdlQtRGB::SdlQtRGB(QWidget *parent) : QWidget(parent)
{
    /// 打开yuv文件
    yuv_file.open(R"(.\assert\400_300_25.yuv)", std::ios::binary);
    if (!yuv_file)
    {
        QMessageBox::information(this, "", "open yuv failed!");
        return;
    }


    ui_ = new Ui::SdlQtRGBClass;
    ui_->setupUi(this);

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


    startTimer(10);
}

SdlQtRGB::~SdlQtRGB()
{
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

    QWidget::timerEvent(event);
}

void SdlQtRGB::resizeEvent(QResizeEvent *event)
{
    ui_->label->resize(size());
    ui_->label->move(0, 0);
    view->scale(width(), height());
}
