#include "XCameraWidget.h"
#include "RtspClient.h"
#include "XDisplayTask.h"
#include "XVideoView.h"
#include "XCameraConfig.h"

#include <QtWidgets/QListWidget>
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
    bool                        is_loading_ = false;
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
    impl_->rtsp_client_->set_max_reconnects(3);


    /// 设置首帧回调，加载完成后关闭 loading 提示
    impl_->rtsp_client_->setFirstFrameCallback(
            [this]()
            {
                impl_->is_loading_ = false;
                /// 需要在主线程更新 UI
                QMetaObject::invokeMethod(this, [this]() { update(); });
            });

    /// 设置渲染窗口
    if (auto display_task = impl_->rtsp_client_->getDisplayTask())
    {
        display_task->setWindow(reinterpret_cast<void *>(winId()));
    }

    /// 启动播放
    if (!impl_->rtsp_client_->start())
    {
        impl_->rtsp_client_.reset();
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

void XCameraWidget::dragEnterEvent(QDragEnterEvent *event)
{
    event->acceptProposedAction();
}

void XCameraWidget::dropEvent(QDropEvent *event)
{
    if (auto wid = dynamic_cast<QListWidget *>(event->source()))
    {
        int  row    = wid->currentRow();
        auto config = XCameraConfig::instance();
        if (const auto cam = config->getCamera(row))
        {
            /// 先显示 loading
            impl_->is_loading_ = true;
            update();

            std::thread([this, cam]() { open(cam->sub_url); }).detach();
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
