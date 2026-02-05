#include "SdlQtRGB.h"

#include "ui_sdlqtrgb.h"
#include "XVideoView.h"

#include <SDL2/SDL.h>

#include <QtGui/QImage>
#include <QtWidgets/QMessageBox>

#include <algorithm>
#include <fstream>

static int sdl_width  = 0;
static int sdl_height = 0;

static unsigned char *yuv      = NULL;
static int            pix_size = 2;

static std::ifstream yuv_file;
static XVideoView   *view = nullptr;

SdlQtRGB::SdlQtRGB(QWidget *parent) : QWidget(parent)
{
    /// 打开yuv文件
    yuv_file.open(R"(.\assert\400_300_25.yuv)", std::ios::binary);
    if (!yuv_file)
    {
        QMessageBox::information(this, "", "open yuv failed!");
        return;
    }


    ui_ = new Ui::SdlQtRGBClass();
    ui_->setupUi(this);

    sdl_width  = 400;
    sdl_height = 300;
    ui_->label->resize(sdl_width, sdl_height);
    view = XVideoView::create();
    // view->init(sdl_width, sdl_height, XVideoView::YUV420P);
    // view->close();
    view->init(sdl_width, sdl_height, XVideoView::YUV420P, (void *)ui_->label->winId());

    //////////////////////////////////////////////////////////////////

    yuv = new unsigned char[sdl_width * sdl_height * pix_size];

    /// 默认设置为透明
    memset(yuv, 0, sdl_width * sdl_height * pix_size);


    startTimer(10);
}

SdlQtRGB::~SdlQtRGB()
{
}

void SdlQtRGB::timerEvent(QTimerEvent *event)
{
    yuv_file.read((char *)yuv, sdl_width * sdl_height * 1.5);
    if (view->isExit())
    {
        view->close();
        exit(0);
    }
    view->draw(yuv);

    QWidget::timerEvent(event);
}

void SdlQtRGB::resizeEvent(QResizeEvent *event)
{
    ui_->label->resize(size());
    ui_->label->move(0, 0);
    view->scale(width(), height());
}
