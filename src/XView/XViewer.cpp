#include "XViewer.h"

#include "ui_xviewer.h"

#include <QtGui/QMouseEvent>
#include <QtWidgets/QVBoxLayout>

static QWidget *cam_wids[16] = { 0 };

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
    auto hlay = new QHBoxLayout;
    ui_->body->setLayout(hlay);
    /// 边框间距
    hlay->setContentsMargins(0, 0, 0, 0);
    hlay->addWidget(ui_->left);
    hlay->addWidget(ui_->cams);

    //////////////////////////////////////
    /// 初始化右键菜单
    /// 视图=》  1 窗口
    ///          4 窗口
    auto m = left_menu_.addMenu("视图");
    auto a = m->addAction("1窗口");
    connect(a, SIGNAL(triggered()), this, SLOT(View1()));
    a = m->addAction("4窗口");
    connect(a, SIGNAL(triggered()), this, SLOT(View4()));
    a = m->addAction("9窗口");
    connect(a, SIGNAL(triggered()), this, SLOT(View9()));
    a = m->addAction("16窗口");
    connect(a, SIGNAL(triggered()), this, SLOT(View16()));

    /// 默认九窗口
    View9();
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

void XViewer::contextMenuEvent(QContextMenuEvent *event)
{
    /// 鼠标位置显示右键菜单
    left_menu_.exec(QCursor::pos());
    event->accept();
}

void XViewer::View(int count)
{
    qDebug() << "View" << count;
    /// 2X2 3X3 4X4
    /// 确定列数 根号
    int cols = sqrt(count);
    /// 总窗口数量
    int wid_size = sizeof(cam_wids) / sizeof(QWidget *);

    /// 初始化布局器
    auto lay = (QGridLayout *)ui_->cams->layout();
    if (!lay)
    {
        lay = new QGridLayout;
        lay->setContentsMargins(0, 0, 0, 0);
        lay->setSpacing(2); /// 元素间距
        ui_->cams->setLayout(lay);
    }
    /// 初始化窗口
    for (int i = 0; i < count; i++)
    {
        if (!cam_wids[i])
        {
            cam_wids[i] = new QWidget();
            cam_wids[i]->setStyleSheet("background-color:rgb(51,51,51);");
        }
        lay->addWidget(cam_wids[i], i / cols, i % cols);
    }

    /// 清理多余的窗体
    for (int i = count; i < wid_size; i++)
    {
        if (cam_wids[i])
        {
            delete cam_wids[i];
            cam_wids[i] = nullptr;
        }
    }
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

void XViewer::View1()
{
    View(1);
}

void XViewer::View4()
{
    View(4);
}

void XViewer::View9()
{
    View(9);
}

void XViewer::View16()
{
    View(16);
}
