#include "XViewer.h"

#include "ui_xviewer.h"
#include "XCameraConfig.h"

#include <QtGui/QMouseEvent>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QDialog>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QLineEdit>

#define CAM_CONF_PATH "cams.db"

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


    /// 刷新左侧摄像机列表
    XCameraConfig::instance()->load(CAM_CONF_PATH);
    // {
    //     XCameraData cd;
    //     strcpy(cd.name, "camera1");
    //     strcpy(cd.save_path, ".\\camera1\\");
    //     strcpy(cd.url, "rtsp://test:x12345678@192.168.2.64/h264/ch1/main/av_stream");
    //     strcpy(cd.sub_url, "rtsp://test:x12345678@192.168.2.64/h264/ch1/sub/av_stream");
    //     XCameraConfig::instance()->addCamera(cd);
    // }

    refreshCameras();
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

void XViewer::view(int count)
{
    qDebug() << "view" << count;
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

void XViewer::refreshCameras()
{
    auto c = XCameraConfig::instance();
    ui_->cam_list->clear();
    int count = c->getCameraCount();
    for (int i = 0; i < count; i++)
    {
        auto cam  = c->getCamera(i);
        auto item = new QListWidgetItem(QIcon(":/XViewer/img/cam.png"), cam->name);
        ui_->cam_list->addItem(item);
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
    view(1);
}

void XViewer::View4()
{
    view(4);
}

void XViewer::View9()
{
    view(9);
}

void XViewer::View16()
{
    view(16);
}

void XViewer::AddCam()
{
    QDialog dlg(this);
    dlg.resize(800, 200);
    QFormLayout lay;
    dlg.setLayout(&lay);
    ///  标题1 输入框1
    ///  标题2 输入框2
    QLineEdit name_edit;
    lay.addRow("名称", &name_edit);

    QLineEdit url_edit;
    lay.addRow("主码流", &url_edit);

    QLineEdit sub_url_edit;
    lay.addRow("辅码流", &sub_url_edit);

    QLineEdit save_path_edit;
    lay.addRow("保存目录", &save_path_edit);

    QPushButton save;
    save.setText("保存");

    connect(&save, SIGNAL(clicked()), &dlg, SLOT(accept()));

    lay.addRow("", &save);
    for (;;)
    {
        if (dlg.exec() == QDialog::Accepted) /// 点击了保存
        {
            if (name_edit.text().isEmpty())
            {
                QMessageBox::information(0, "error", "请输入名称");
                continue;
            }
            if (url_edit.text().isEmpty())
            {
                QMessageBox::information(0, "error", "请输入主码流");
                continue;
            }
            if (sub_url_edit.text().isEmpty())
            {
                QMessageBox::information(0, "error", "请输入辅码流");
                continue;
            }
            if (save_path_edit.text().isEmpty())
            {
                QMessageBox::information(0, "error", "请输入保存目录");
                continue;
            }
            break;
        }
        return;
    }

    XCameraData data;
    strcpy(data.name, name_edit.text().toLocal8Bit());
    strcpy(data.url, url_edit.text().toLocal8Bit());
    strcpy(data.sub_url, sub_url_edit.text().toLocal8Bit());
    strcpy(data.save_path, save_path_edit.text().toLocal8Bit());
    XCameraConfig::instance()->addCamera(data);     /// 插入数据
    XCameraConfig::instance()->save(CAM_CONF_PATH); /// 保存到文件
    refreshCameras();                               /// 刷新显示
}
