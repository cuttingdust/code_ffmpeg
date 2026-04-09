#include "XCameraWidget.h"
#include "RtspClient.h"
#include "XDisplayTask.h"
#include "XVideoView.h"
#include "XCameraConfig.h"

#include <QtWidgets/QListWidget>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QApplication>
#include <QtGui/QtEvents>
#include <QtGui/QPainter>
#include <QtCore/QTimer>

class XCameraWidget::PImpl
{
public:
    PImpl(XCameraWidget *owenr);
    ~PImpl() = default;

public:
    XCameraWidget              *owenr_ = nullptr;
    std::shared_ptr<RtspClient> rtsp_client_;
    QString                     current_url_;
    bool                        is_loading_    = false;
    QTimer                     *loading_timer_ = nullptr;
    int                         camera_id_     = -1;
};

XCameraWidget::PImpl::PImpl(XCameraWidget *owenr) : owenr_(owenr)
{
}


XCameraWidget::XCameraWidget(QWidget *p) : QWidget(p), impl_(std::make_unique<XCameraWidget::PImpl>(this))
{
    setAcceptDrops(true);
    setAttribute(Qt::WA_NativeWindow); /// 确保有原生窗口句柄
}

XCameraWidget::~XCameraWidget()
{
    close();
}

bool XCameraWidget::open(const QString &url)
{
    /// 关闭已有的播放
    if (impl_->rtsp_client_ && impl_->rtsp_client_->isRunning())
    {
        close();
    }

    impl_->current_url_ = url;
    impl_->rtsp_client_ = std::make_shared<RtspClient>();
    impl_->rtsp_client_->setUrl(url.toStdString());
    impl_->rtsp_client_->setReconnectInterval(5);
    impl_->rtsp_client_->setMaxReconnects(3);
    impl_->rtsp_client_->setRenderWindow(reinterpret_cast<void *>(winId()));

    /// 显示加载提示
    impl_->is_loading_ = true;
    update();

    /// 启动超时定时器（8秒）
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

    /// 设置首帧回调，加载完成后关闭 loading 提示
    impl_->rtsp_client_->setFirstFrameCallback(
            [this]()
            {
                LOGI("首帧回调被触发"); // 添加这行日志
                QMetaObject::invokeMethod(this,
                                          [this]()
                                          {
                                              LOGI("主线程执行首帧回调"); // 添加这行日志
                                              if (impl_->loading_timer_)
                                              {
                                                  impl_->loading_timer_->stop();
                                                  LOGI("定时器已停止");
                                              }
                                              impl_->is_loading_ = false;
                                              update();
                                          });
            });

    /// 启动播放
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

void XCameraWidget::close() const
{
    if (impl_->rtsp_client_)
    {
        impl_->rtsp_client_->stop();
        impl_->rtsp_client_->wait();
        impl_->rtsp_client_.reset();
    }
}

auto XCameraWidget::setCameraId(int id) -> void
{
    impl_->camera_id_ = id;
}

auto XCameraWidget::getCameraId() const -> int
{
    return impl_->camera_id_;
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
            /// 先显示 loading
            impl_->is_loading_ = true;
            update();

            /// 强制立即处理重绘事件，确保"加载中..."立即显示
            QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

            /// 使用 QTimer 延迟执行，不阻塞 UI
            QTimer::singleShot(0, this, [this, url = QString::fromStdString(cam->sub_url)]() { open(url); });
        }
    }
}

void XCameraWidget::paintEvent(QPaintEvent *event)
{
    /// 渲染样式表
    QStyleOption opt;
    opt.initFrom(this);
    QPainter painter(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &painter, this);

    /// 如果正在加载，显示提示文字
    if (impl_->is_loading_)
    {
        painter.setPen(Qt::white);
        painter.drawText(rect(), Qt::AlignCenter, "加载中...");
    }
}

void XCameraWidget::resizeEvent(QResizeEvent *event)
{
    /// 通知渲染器调整大小
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
