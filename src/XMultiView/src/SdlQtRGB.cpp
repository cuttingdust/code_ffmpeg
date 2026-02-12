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
#include <QtWidgets/QFileDialog>

#include <fstream>
#include <iostream>
#include <algorithm>

extern void                      MSleep(unsigned int ms);
extern long long                 NowMs();
static std::vector<XVideoView *> views;

SdlQtRGB::SdlQtRGB(QWidget *parent) : QWidget(parent)
{
    ui_ = new Ui::SdlQtRGBClass;
    ui_->setupUi(this);

    /// 绑定渲染信号槽
    connect(this, &SdlQtRGB::signalView, this, &SdlQtRGB::slotView);
    views.push_back(XVideoView::create());
    views.push_back(XVideoView::create());
    views[0]->setWindow((void *)ui_->video1->winId());
    views[1]->setWindow((void *)ui_->video2->winId());
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
        MSleep(10);
    }
}

void SdlQtRGB::open(int i)
{
    auto filename = QFileDialog::getOpenFileName();
    if (filename.isEmpty())
    {
        return;
    }
    std::cout << filename.toStdString() << std::endl;
    /// 打开文件
    if (!views[i]->open(filename.toStdString()))
    {
        return;
    }

    int     w   = 0;
    int     h   = 0;
    QString pix = 0; /// YUV420P RGBA
    if (i == 0)
    {
        w   = ui_->width1->value();
        h   = ui_->height1->value();
        pix = ui_->pix1->currentText(); /// 像素格式
    }
    else
    {
        w   = ui_->width2->value();
        h   = ui_->height2->value();
        pix = ui_->pix2->currentText();
    }

    XVideoView::Format fmt = XVideoView::YUV420P;
    if (pix == "YUV420P")
    {
        fmt = XVideoView::YUV420P;
    }
    else if (pix == "RGBA")
    {
        fmt = XVideoView::RGBA;
    }
    else if (pix == "ARGB")
    {
        fmt = XVideoView::ARGB;
    }
    else if (pix == "BGRA")
    {
        fmt = XVideoView::BGRA;
    }

    /// 初始化窗口和材质
    views[i]->init(w, h, fmt);
}

void SdlQtRGB::timerEvent(QTimerEvent *event)
{
}

void SdlQtRGB::resizeEvent(QResizeEvent *event)
{
}

void SdlQtRGB::slotView()
{
    /// 存放上次渲染的时间戳
    static int last_pts[32] = { 0 };
    static int fps_arr[32]  = { 0 };
    fps_arr[0]              = ui_->set_fps1->value();
    fps_arr[1]              = ui_->set_fps2->value();


    for (int i = 0; i < views.size(); ++i)
    {
        if (fps_arr[i] <= 0)
        {
            continue;
        }

        /// 需要间隔时间
        int ms = 1000 / fps_arr[i];

        /// 判断是否到了可渲染时间
        if (NowMs() - last_pts[i] < ms)
        {
            continue;
        }
        last_pts[i] = NowMs();

        auto frame = views[i]->read();
        if (!frame)
        {
            continue;
        }
        views[i]->drawFrame(frame);
        /// 显示fps
        std::stringstream ss;
        ss << "fps:" << views[i]->renderFps();
        if (i == 0)
        {
            ui_->fps1->setText(ss.str().c_str());
        }
        else
        {
            ui_->fps2->setText(ss.str().c_str());
        }
    }
}

void SdlQtRGB::Open1()
{
    open(0);
}

void SdlQtRGB::Open2()
{
    open(1);
}
