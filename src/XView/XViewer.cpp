#include "XViewer.h"

#include "ui_xviewer.h"

#include <QtGui/QMouseEvent>
#include <QtWidgets/QVBoxLayout>

XViewer::XViewer(QWidget *parent) : QWidget(parent)
{
    ui_ = new Ui::XViewerClass;
    ui_->setupUi(this);
    ui_->head->installEventFilter(this);
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint);
    // setAttribute(Qt::WA_TranslucentBackground, true);

    /// 布局head和body 垂直布局器
    auto vlay = new QVBoxLayout();
    /// 边框间距
    vlay->setContentsMargins(0, 0, 0, 0);
    /// 元素间距
    vlay->setSpacing(0);
    vlay->addWidget(ui_->head);
    vlay->addWidget(ui_->body);
    this->setLayout(vlay);

    ///相机列表 和相机预览
    ///水平布局器
    auto hlay = new QHBoxLayout();
    ui_->body->setLayout(hlay);
    /// 边框间距
    hlay->setContentsMargins(0, 0, 0, 0);
    hlay->addWidget(ui_->left);
    hlay->addWidget(ui_->cams);
}

bool XViewer::eventFilter(QObject *pObj, QEvent *pEvent)
{
    static QPoint mousePoint;
    static bool   mousePressed = false;

    QMouseEvent *event = static_cast<QMouseEvent *>(pEvent);
    if (event->type() == QEvent::MouseButtonPress)
    {
        if (event->button() == Qt::LeftButton)
        {
            mousePressed = true;
            mousePoint   = event->globalPos() - this->pos();
            return true;
        }
    }
    else if (event->type() == QEvent::MouseButtonRelease)
    {
        mousePressed = false;
        return true;
    }
    else if (event->type() == QEvent::MouseMove)
    {
        if (mousePressed && (event->buttons() & Qt::LeftButton))
        {
            this->move(event->globalPos() - mousePoint);
            return true;
        }
    }

    return QWidget::eventFilter(pObj, pEvent);
}

void XViewer::resizeEvent(QResizeEvent *event)
{
    int x = width() - ui_->head_button->width();
    int y = ui_->head_button->y();
    ui_->head_button->move(x, y);
}

void XViewer::MaxWindow()
{
    ui_->max->setVisible(false);
    ui_->normal->setVisible(true);
    showMaximized();
}

void XViewer::NormalWindow()
{
    ui_->max->setVisible(true);
    ui_->normal->setVisible(false);
    showNormal();
}
