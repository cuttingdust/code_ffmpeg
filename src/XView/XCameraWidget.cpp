#include "XCameraWidget.h"

#include "RecordClient.h"
#include "RtspClient.h"
#include "XOpenGLVideoWidget.h"
#include "XCameraConfig.h"
#include "XRecorderManager.h"
#include "XOverlayUtil.h"

#include <filesystem>

#include <QtWidgets/QListWidget>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMenu>
#include <QtWidgets/QVBoxLayout>
#include <QtGui/QtEvents>
#include <QtCore/QTimer>

class XCameraWidget::PImpl
{
public:
    PImpl(XCameraWidget *owenr);
    ~PImpl() = default;

public:
    XCameraWidget                *owenr_        = nullptr;
    XOpenGLVideoWidget           *video_widget_ = nullptr;
    std::shared_ptr<RtspClient>   rtsp_client_;
    QString                       current_url_;
    bool                          is_loading_    = false;
    QTimer                       *loading_timer_ = nullptr;
    int                           camera_id_     = -1;
    std::shared_ptr<RecordClient> record_client_;
};

XCameraWidget::PImpl::PImpl(XCameraWidget *owenr) : owenr_(owenr)
{
}

XCameraWidget::XCameraWidget(QWidget *p) : QWidget(p), impl_(std::make_unique<XCameraWidget::PImpl>(this))
{
    setAcceptDrops(true);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    impl_->video_widget_ = new XOpenGLVideoWidget(this);
    impl_->video_widget_->setOverlayStyle(defaultRecOverlayStyle());
    layout->addWidget(impl_->video_widget_);
}

XCameraWidget::~XCameraWidget()
{
    // 释放摄像头
    if (impl_->camera_id_ >= 0)
    {
        emit cameraReleased(impl_->camera_id_);
    }
    stop();
}

bool XCameraWidget::isPlaying() const
{
    return impl_->rtsp_client_ && impl_->rtsp_client_->isRunning();
}

QString XCameraWidget::getCameraName() const
{
    if (impl_->camera_id_ < 0)
    {
        return QString();
    }
    auto config = XCameraConfig::instance();
    auto cam    = config->getCamera(impl_->camera_id_);
    if (cam)
    {
        return QString::fromStdString(cam->name);
    }
    return QString();
}

void XCameraWidget::updateMenuState()
{
    if (!context_menu_)
    {
        return;
    }

    bool playing = isPlaying();
    const bool has_audio = impl_->rtsp_client_ && impl_->rtsp_client_->hasAudio();

    if (start_record_action_)
    {
        start_record_action_->setEnabled(playing && !isRecording());
    }
    if (stop_record_action_)
    {
        stop_record_action_->setEnabled(playing && isRecording());
    }
    if (enable_audio_action_)
    {
        enable_audio_action_->setVisible(has_audio);
        enable_audio_action_->setEnabled(playing && has_audio && !audio_enabled_);
    }
    if (disable_audio_action_)
    {
        disable_audio_action_->setVisible(has_audio);
        disable_audio_action_->setEnabled(playing && has_audio && audio_enabled_);
    }
}

bool XCameraWidget::open(const QString &url)
{
    if (impl_->rtsp_client_ && impl_->rtsp_client_->isRunning())
    {
        stop();
    }

    impl_->current_url_ = url;
    impl_->rtsp_client_ = std::make_shared<RtspClient>();
    impl_->rtsp_client_->setUrl(url.toStdString());
    impl_->rtsp_client_->setReconnectInterval(5);
    impl_->rtsp_client_->setMaxReconnects(3);
    impl_->rtsp_client_->setOpenGLWidget(impl_->video_widget_);
    impl_->rtsp_client_->setRenderBackend(RenderBackend::OpenGL);
    impl_->rtsp_client_->setOverlayStyle(defaultRecOverlayStyle());

    impl_->video_widget_->init();

    impl_->is_loading_ = true;
    if (impl_->video_widget_)
    {
        impl_->video_widget_->setOverlayMessage(tr("加载中..."));
    }
    update();

    if (!impl_->loading_timer_)
    {
        impl_->loading_timer_ = new QTimer(this);
        impl_->loading_timer_->setSingleShot(true);
        connect(impl_->loading_timer_, &QTimer::timeout, this,
                [this]()
                {
                    if (impl_->is_loading_)
                    {
                        impl_->is_loading_ = false;
                        QMessageBox::warning(this, "连接超时",
                                             "无法连接到:\n" + impl_->current_url_ + "\n\n请检查网络和摄像头配置");
                        update();
                    }
                });
    }
    impl_->loading_timer_->start(8000);

    impl_->video_widget_->setFirstFrameCallback(
            [this]()
            {
                QMetaObject::invokeMethod(this,
                                          [this]()
                                          {
                                              if (impl_->loading_timer_)
                                              {
                                                  impl_->loading_timer_->stop();
                                              }
                                              impl_->is_loading_ = false;
                                              if (impl_->video_widget_)
                                              {
                                                  impl_->video_widget_->setOverlayMessage(QString());
                                              }
                                              if (impl_->rtsp_client_ && impl_->rtsp_client_->hasAudio()
                                                  && !audio_enabled_)
                                              {
                                                  setAudioEnabled(true);
                                              }
                                              updateMenuState();
                                              update();
                                          });
            });

    if (!impl_->rtsp_client_->start())
    {
        if (impl_->loading_timer_)
        {
            impl_->loading_timer_->stop();
        }

        impl_->rtsp_client_.reset();
        impl_->is_loading_ = false;
        QMessageBox::warning(this, "启动失败", "无法启动播放器:\n" + url);
        update();
        return false;
    }

    return true;
}

void XCameraWidget::stop()
{
    // 停止播放
    if (impl_->rtsp_client_)
    {
        impl_->rtsp_client_->stop();
        impl_->rtsp_client_->wait();
        impl_->rtsp_client_.reset();
    }
    audio_enabled_ = false;

    // 停止录制（如果正在录制）
    if (impl_->camera_id_ >= 0 && isRecording())
    {
        XRecorderManager::instance().stopRecording(impl_->camera_id_);
    }

    updateMenuState();
    update();
}

void XCameraWidget::setCameraId(int id)
{
    int old_id = impl_->camera_id_;
    if (old_id >= 0)
    {
        emit cameraReleased(old_id);
    }

    impl_->camera_id_ = id;

    if (id >= 0)
    {
        emit cameraAssigned(id);
    }
}

int XCameraWidget::getCameraId() const
{
    return impl_->camera_id_;
}

void XCameraWidget::startRecording()
{
    if (impl_->camera_id_ < 0)
    {
        QMessageBox::warning(this, "错误", "请先拖拽摄像机到窗口");
        return;
    }

    if (!isPlaying())
    {
        QMessageBox::warning(this, "错误", "请先等待视频播放");
        return;
    }

    if (isRecording())
    {
        QMessageBox::information(this, "提示", "已在录制中");
        return;
    }

    EncoderConfig config;
    config.codec_id       = AV_CODEC_ID_H264;
    config.width          = 400;
    config.height         = 300;
    config.bitrate        = 0;
    config.framerate      = 25;
    config.gop_size       = 25;
    config.max_b_frames   = 0;
    config.pix_fmt        = AV_PIX_FMT_YUV420P;
    config.h264.preset    = "medium";
    config.h264.profile   = "high";
    config.h264.crf       = 18;
    config.h264.force_idr = true;
    config.thread_count   = 4;

    if (!XRecorderManager::instance().startRecording(impl_->camera_id_, config))
    {
        QMessageBox::warning(this, "错误", "启动录制失败");
        return;
    }

    QMessageBox::information(this, "提示", "开始录制");

    // 设置录制指示器
    setRecordingIndicatorFromManager(true);

    emit recordingStateChanged(impl_->camera_id_, true);
    updateMenuState();
    update();
}

void XCameraWidget::stopRecording()
{
    if (impl_->camera_id_ < 0)
    {
        return;
    }

    if (!isRecording())
    {
        return;
    }

    XRecorderManager::instance().stopRecording(impl_->camera_id_);

    // 清除录制指示器
    setRecordingIndicatorFromManager(false);

    QMessageBox::information(this, "提示", "停止录制");
    emit recordingStateChanged(impl_->camera_id_, false);
    updateMenuState();
    update();
}

bool XCameraWidget::isRecording() const
{
    if (impl_->camera_id_ < 0)
    {
        return false;
    }
    return XRecorderManager::instance().isRecording(impl_->camera_id_);
}

void XCameraWidget::setRecordingIndicatorFromManager(bool recording)
{
    is_recording_flag_ = recording;

    if (impl_->rtsp_client_)
    {
        impl_->rtsp_client_->setRecordingIndicator(recording);
    }
}

auto XCameraWidget::getRtspClient() const -> std::shared_ptr<RtspClient>
{
    return impl_->rtsp_client_;
}

bool XCameraWidget::isAudioEnabled() const
{
    return audio_enabled_;
}

void XCameraWidget::setAudioEnabled(bool enabled)
{
    if (!impl_->rtsp_client_ || !impl_->rtsp_client_->hasAudio())
    {
        audio_enabled_ = false;
        return;
    }

    if (enabled)
    {
        emit exclusiveAudioRequested(impl_->camera_id_);
        if (impl_->rtsp_client_->enableAudio())
        {
            impl_->rtsp_client_->setVolume(1.0);
            audio_enabled_ = true;
        }
    }
    else
    {
        impl_->rtsp_client_->disableAudio();
        audio_enabled_ = false;
    }

    updateMenuState();
}

void XCameraWidget::muteAudio()
{
    if (impl_->rtsp_client_)
    {
        impl_->rtsp_client_->disableAudio();
    }
    audio_enabled_ = false;
    updateMenuState();
}

void XCameraWidget::pausePreviewAudio()
{
    if (impl_->rtsp_client_)
    {
        impl_->rtsp_client_->pauseAudio();
    }
}

void XCameraWidget::resumePreviewAudio()
{
    if (!audio_enabled_ || !impl_->rtsp_client_)
    {
        return;
    }

    impl_->rtsp_client_->resumeAudio();
    impl_->rtsp_client_->setVolume(1.0);
}

void XCameraWidget::contextMenuEvent(QContextMenuEvent *event)
{
    if (!context_menu_)
    {
        context_menu_ = new QMenu(this);

        auto viewMenu = context_menu_->addMenu("视图");
        viewMenu->addAction("1窗口", this, [this]() { emit changeViewMode(1); });
        viewMenu->addAction("4窗口", this, [this]() { emit changeViewMode(4); });
        viewMenu->addAction("9窗口", this, [this]() { emit changeViewMode(9); });
        viewMenu->addAction("16窗口", this, [this]() { emit changeViewMode(16); });

        context_menu_->addSeparator();

        auto recordMenu      = context_menu_->addMenu("录制");
        start_record_action_ = recordMenu->addAction("开始录制", this, &XCameraWidget::startRecording);
        stop_record_action_  = recordMenu->addAction("停止录制", this, &XCameraWidget::stopRecording);

        context_menu_->addSeparator();

        enable_audio_action_  = context_menu_->addAction("开启声音", this, [this]() { setAudioEnabled(true); });
        disable_audio_action_ = context_menu_->addAction("关闭声音", this, [this]() { setAudioEnabled(false); });

        updateMenuState();
    }

    updateMenuState();
    context_menu_->exec(event->globalPos());
}

void XCameraWidget::dragEnterEvent(QDragEnterEvent *event)
{
    event->acceptProposedAction();
}

void XCameraWidget::dropEvent(QDropEvent *event)
{
    if (auto wid = dynamic_cast<QListWidget *>(event->source()))
    {
        int row = wid->currentRow();

        // 先释放旧的摄像头
        if (impl_->camera_id_ >= 0)
        {
            emit cameraReleased(impl_->camera_id_);
        }

        impl_->camera_id_ = row;

        // 通知新摄像头被分配
        emit cameraAssigned(row);

        auto config = XCameraConfig::instance();
        if (const auto cam = config->getCamera(row))
        {
            impl_->is_loading_ = true;
            update();

            QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

            QTimer::singleShot(0, this,
                               [this, url = QString::fromStdString(cam->sub_url)]()
                               {
                                   open(url);

                                   // 打开后检查录制状态，如果正在录制则显示 REC
                                   if (XRecorderManager::instance().isRecording(impl_->camera_id_))
                                   {
                                       setRecordingIndicatorFromManager(true);
                                   }
                               });
        }
    }
}

void XCameraWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
}

void XCameraWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
}
