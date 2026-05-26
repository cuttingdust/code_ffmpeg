#include "XRtspDemo.h"
#include "ui_XRtspDemo.h"

#include "RtspClient.h"
#include "XOpenGLVideoWidget.h"
#include "XMediaClient.h"
#include "XOverlayUtil.h"

#include <QtCore/QTimer>

namespace
{
constexpr const char* kDefaultRtspUrl = "rtsp://localhost:8554/test";
constexpr int         kRecordSeconds  = 20;
} // namespace

XRtspDemo::XRtspDemo(QWidget* parent) : QWidget(parent)
{
    ui = new Ui::XRtspDemoClass();
    ui->setupUi(this);
    initClient();
}

XRtspDemo::~XRtspDemo()
{
    if (status_timer_)
    {
        status_timer_->stop();
    }
    if (record_timer_)
    {
        record_timer_->stop();
    }
    if (client_)
    {
        client_->stop();
    }
    delete ui;
}

void XRtspDemo::initClient()
{
    if (!ui->openGLWidget)
    {
        return;
    }

    client_ = RtspClient::create();
    client_->setUrl(kDefaultRtspUrl);
    client_->setReconnectInterval(5);
    client_->setMaxReconnects(3);
    client_->setOpenGLWidget(ui->openGLWidget);
    client_->setRenderBackend(RenderBackend::OpenGL);
    client_->setOverlayStyle(defaultRecOverlayStyle());
    ui->openGLWidget->setOverlayMessage(tr("连接中..."));

    client_->setFirstFrameCallback(
            [this]()
            {
                QMetaObject::invokeMethod(this,
                                          [this]()
                                          {
                                              if (ui->openGLWidget)
                                              {
                                                  ui->openGLWidget->setOverlayMessage(QString());
                                              }
                                              updateStatus();
                                              if (!record_started_)
                                              {
                                                  record_timer_ = new QTimer(this);
                                                  record_timer_->setSingleShot(true);
                                                  connect(record_timer_, &QTimer::timeout, this, &XRtspDemo::startAutoRecord);
                                                  record_timer_->start(2000);
                                              }
                                          },
                                          Qt::QueuedConnection);
            });

    client_->start();

    status_timer_ = new QTimer(this);
    connect(status_timer_, &QTimer::timeout, this, &XRtspDemo::updateStatus);
    status_timer_->start(500);
}

void XRtspDemo::startAutoRecord()
{
    if (!client_ || record_started_)
    {
        return;
    }

    record_started_ = true;
    if (client_->startRecording("output.mp4", kRecordSeconds))
    {
        client_->setRecordingIndicator(true);
        updateStatus();
    }
    else if (ui->statusLabel)
    {
        ui->statusLabel->setText(tr("启动录制失败"));
    }
}

void XRtspDemo::updateStatus()
{
    if (!ui->statusLabel || !client_)
    {
        return;
    }

    const auto state = client_->getState();
    QString    state_text;
    switch (state)
    {
    case MediaClientState::CONNECTING:
        state_text = tr("连接中");
        break;
    case MediaClientState::CONNECTED:
        state_text = tr("已连接");
        break;
    case MediaClientState::RECONNECTING:
        state_text = tr("重连中");
        break;
    case MediaClientState::ERROR:
        state_text = tr("错误");
        break;
    case MediaClientState::DISCONNECTED:
    default:
        state_text = tr("已断开");
        break;
    }

    QString text = tr("状态: %1 | 渲染 FPS: %2").arg(state_text).arg(ui->openGLWidget->renderFps());

    if (client_->isRecording())
    {
        const auto status = client_->getRecordingStatus();
        text += tr(" | 录制: %1/%2 秒, 包数: %3")
                        .arg(status.recorded_sec)
                        .arg(status.total_sec)
                        .arg(status.packet_count);
    }
    else if (record_started_)
    {
        client_->setRecordingIndicator(false);
        text += tr(" | 录制完成: output.mp4");
    }

    ui->statusLabel->setText(text);
}
