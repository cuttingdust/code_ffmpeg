#include "XCameraWidget.h"

#include "RecordClient.h"
#include "RtspClient.h"
#include "XDisplayTask.h"
#include "XVideoView.h"
#include "XCameraConfig.h"
#include "XRecorderManager.h"

#include <filesystem>

#include <QtWidgets/QListWidget>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMenu>
#include <QtGui/QtEvents>
#include <QtGui/QPainter>
#include <QtCore/QTimer>

class XCameraWidget::PImpl
{
public:
    PImpl(XCameraWidget *owenr);
    ~PImpl() = default;

public:
    XCameraWidget                *owenr_ = nullptr;
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
    setAttribute(Qt::WA_NativeWindow);
}

XCameraWidget::~XCameraWidget()
{
    stop();
}

bool XCameraWidget::isPlaying() const
{
    return impl_->rtsp_client_ && impl_->rtsp_client_->isRunning();
}

QString XCameraWidget::getCameraName() const
{
    if (impl_->camera_id_ < 0)
        return QString();
    auto config = XCameraConfig::instance();
    auto cam    = config->getCamera(impl_->camera_id_);
    if (cam)
        return QString::fromStdString(cam->name);
    return QString();
}

void XCameraWidget::updateMenuState()
{
    if (!context_menu_)
        return;

    bool playing = isPlaying();

    if (start_record_action_)
    {
        start_record_action_->setEnabled(playing && !isRecording());
    }
    if (stop_record_action_)
    {
        stop_record_action_->setEnabled(playing && isRecording());
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
    impl_->rtsp_client_->setRenderWindow(reinterpret_cast<void *>(winId()));

    impl_->is_loading_ = true;
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

    impl_->rtsp_client_->setFirstFrameCallback(
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

    // 停止录制（如果正在录制）
    if (impl_->camera_id_ >= 0 && isRecording())
    {
        XRecorderManager::instance().stopRecording(impl_->camera_id_);
    }

    updateMenuState();
}

void XCameraWidget::setCameraId(int id)
{
    impl_->camera_id_ = id;
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

    // 检查是否已经在录制
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
    }
    else
    {
        QMessageBox::information(this, "提示", "开始录制");
        emit recordingStateChanged(impl_->camera_id_, true);
        updateMenuState();
        update();
    }
}

void XCameraWidget::stopRecording()
{
    if (impl_->camera_id_ < 0)
        return;

    if (!isRecording())
        return;

    int camera_id = impl_->camera_id_;
    XRecorderManager::instance().stopRecording(camera_id);
    QMessageBox::information(this, "提示", "停止录制");
    emit recordingStateChanged(camera_id, false);
    updateMenuState();
    update();
}

bool XCameraWidget::isRecording() const
{
    if (impl_->camera_id_ < 0)
        return false;
    return XRecorderManager::instance().isRecording(impl_->camera_id_);
}

void XCameraWidget::contextMenuEvent(QContextMenuEvent *event)
{
    if (!context_menu_)
    {
        context_menu_ = new QMenu(this);

        // 视图子菜单
        auto viewMenu = context_menu_->addMenu("视图");
        viewMenu->addAction("1窗口", this, [this]() { emit changeViewMode(1); });
        viewMenu->addAction("4窗口", this, [this]() { emit changeViewMode(4); });
        viewMenu->addAction("9窗口", this, [this]() { emit changeViewMode(9); });
        viewMenu->addAction("16窗口", this, [this]() { emit changeViewMode(16); });

        context_menu_->addSeparator();

        // 录制子菜单
        auto recordMenu      = context_menu_->addMenu("录制");
        start_record_action_ = recordMenu->addAction("开始录制", this, &XCameraWidget::startRecording);
        stop_record_action_  = recordMenu->addAction("停止录制", this, &XCameraWidget::stopRecording);

        updateMenuState();
    }

    // 更新菜单状态
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
        int row           = wid->currentRow();
        impl_->camera_id_ = row;

        auto config = XCameraConfig::instance();
        if (const auto cam = config->getCamera(row))
        {
            impl_->is_loading_ = true;
            update();

            QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

            QTimer::singleShot(0, this, [this, url = QString::fromStdString(cam->sub_url)]() { open(url); });
        }
    }
}

void XCameraWidget::paintEvent(QPaintEvent *event)
{
    QStyleOption opt;
    opt.initFrom(this);
    QPainter painter(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &painter, this);

    if (impl_->is_loading_)
    {
        painter.setPen(Qt::white);
        painter.drawText(rect(), Qt::AlignCenter, "加载中...");
    }

    if (isRecording())
    {
        painter.save();

        painter.setBrush(QColor(0, 0, 0, 180));
        painter.setPen(Qt::NoPen);
        painter.drawRoundedRect(5, 5, 65, 28, 6, 6);

        painter.setBrush(Qt::red);
        painter.drawEllipse(15, 11, 10, 10);

        painter.setPen(Qt::white);
        painter.setFont(QFont("Arial", 10, QFont::Bold));
        painter.drawText(32, 18, "REC");

        painter.restore();
    }
}

void XCameraWidget::resizeEvent(QResizeEvent *event)
{
    if (impl_->rtsp_client_ && impl_->rtsp_client_->isRunning())
    {
        if (auto display_task = impl_->rtsp_client_->getDisplayTask())
        {
            if (auto view = display_task->getVideoView())
            {
                view->scale(event->size().width(), event->size().height());
            }
        }
    }
}
