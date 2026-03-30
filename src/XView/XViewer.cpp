#include "XViewer.h"

#include "ui_xviewer.h"

#include <QtGui/QMouseEvent>


XViewer::XViewer(QWidget *parent) : QWidget(parent)
{
    ui_ = new Ui::XViewerClass;
    ui_->setupUi(this);
    ui_->head->installEventFilter(this);
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint);
    // setAttribute(Qt::WA_TranslucentBackground, true);
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
