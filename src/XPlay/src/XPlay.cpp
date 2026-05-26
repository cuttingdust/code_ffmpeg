#include "XPlay.h"
#include "ui_XPlay.h"

#include "LocalPlayer.h"
#include "XOpenGLVideoWidget.h"
#include "XOverlayUtil.h"

#include <QtCore/QTimer>

XPlay::XPlay(QWidget* parent) : QWidget(parent)
{
    ui = new Ui::XPlayClass();
    ui->setupUi(this);
    initPlayer();
}

XPlay::~XPlay()
{
    if (fps_timer_)
    {
        fps_timer_->stop();
    }
    if (player_)
    {
        player_->stop();
    }
    delete ui;
}

void XPlay::initPlayer()
{
    if (!ui->openGLWidget)
    {
        return;
    }

    player_ = std::make_unique<LocalPlayer>();
    player_->setOpenGLWidget(ui->openGLWidget);
    player_->setRenderBackend(RenderBackend::OpenGL);
    player_->setOverlayStyle(defaultRecOverlayStyle());

    const std::string filepath = "assert/output.mp4";
    if (!player_->open(filepath))
    {
        ui->statusLabel->setText(QString("打开失败: %1").arg(QString::fromStdString(filepath)));
        return;
    }

    ui->statusLabel->setText(QString("时长: %1 秒").arg(player_->getDuration(), 0, 'f', 2));
    player_->play();

    fps_timer_ = new QTimer(this);
    connect(fps_timer_, &QTimer::timeout, this,
            [this]()
            {
                if (!player_)
                {
                    return;
                }
                ui->statusLabel->setText(QString("渲染 FPS: %1 | 播放中: %2 | 结束: %3")
                                                 .arg(ui->openGLWidget->renderFps())
                                                 .arg(player_->isPlaying() ? "是" : "否")
                                                 .arg(player_->isFinished() ? "是" : "否"));
            });
    fps_timer_->start(500);
}
