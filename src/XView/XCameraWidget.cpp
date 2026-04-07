#include "XCameraWidget.h"

#include <QtWidgets/QListWidget>
#include <QtGui/QtEvents>
#include <QtGui/QPainter>

XCameraWidget::XCameraWidget(QWidget *p)
{
    setAcceptDrops(true);
}

void XCameraWidget::dragEnterEvent(QDragEnterEvent *event)
{
    /// 接收拖拽进入
    event->acceptProposedAction();
}

void XCameraWidget::dropEvent(QDropEvent *event)
{
    /// 拿到url
    qDebug() << event->source()->objectName();
    auto wid = static_cast<QListWidget *>(event->source());
    qDebug() << wid->currentRow();
}

void XCameraWidget::paintEvent(QPaintEvent *event)
{
    /// 渲染样式表
    QStyleOption opt;
    opt.initFrom(this);
    QPainter painter(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &painter, this);
}
