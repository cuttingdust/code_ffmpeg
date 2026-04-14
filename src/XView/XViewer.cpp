#include "XViewer.h"
#include "ui_xviewer.h"
#include "XCameraConfig.h"
#include "XCameraWidget.h"
#include "XRecorderManager.h"

#include <qdir.h>

#include <QtGui/QMouseEvent>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QDialog>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QLineEdit>

#define CAM_CONF_PATH "cams.db"

static XCameraWidget *cam_wids[16] = { 0 };

XViewer::XViewer(QWidget *parent) : QWidget(parent), ui(new Ui::XViewerClass)
{
    ui->setupUi(this);
    ui->head->installEventFilter(this);
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint);

    /// 布局head和body 垂直布局器
    auto vlay = new QVBoxLayout();
    vlay->setContentsMargins(0, 0, 0, 0);
    vlay->setSpacing(0);
    vlay->addWidget(ui->head);
    vlay->addWidget(ui->body);
    this->setLayout(vlay);

    /// 相机列表和相机预览 水平布局器
    auto hlay = new QHBoxLayout;
    ui->body->setLayout(hlay);
    hlay->setContentsMargins(0, 0, 0, 0);
    hlay->addWidget(ui->left);         /// 左侧相机列表
    hlay->addWidget(ui->cams);         /// 右侧预览窗口
    hlay->addWidget(ui->playback_wid); /// 回放窗口

    /// 初始化右键菜单 - 视图
    auto m = left_menu_.addMenu("视图");
    auto a = m->addAction("1窗口");
    connect(a, &QAction::triggered, this, &XViewer::View1);
    a = m->addAction("4窗口");
    connect(a, &QAction::triggered, this, &XViewer::View4);
    a = m->addAction("9窗口");
    connect(a, &QAction::triggered, this, &XViewer::View9);
    a = m->addAction("16窗口");
    connect(a, &QAction::triggered, this, &XViewer::View16);

    /// 默认九窗口
    View9();

    /// 刷新左侧摄像机列表
    XCameraConfig::instance()->load(CAM_CONF_PATH);
    refreshCameras();
    Playback();

    // 注册录制状态回调
    XRecorderManager::instance().registerCallback(
            [this](int camera_id, bool is_recording)
            {
                // 在主线程中更新 UI
                QMetaObject::invokeMethod(this, [this, camera_id, is_recording]()
                                          { onRecordingStatusChanged(camera_id, is_recording); });
            });
}

XViewer::~XViewer()
{
    delete ui;
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
    int x = width() - ui->head_button->width();
    int y = ui->head_button->y();
    ui->head_button->move(x, y);
}

void XViewer::contextMenuEvent(QContextMenuEvent *event)
{
    left_menu_.exec(QCursor::pos());
    event->accept();
}

void XViewer::onRecordingStatusChanged(int camera_id, bool is_recording)
{
    // 更新左侧列表的显示
    refreshCameras();

    // 更新所有播放这个摄像头的窗口的 REC 显示
    updateCameraRecIndicator(camera_id, is_recording);
}

void XViewer::updateCameraRecIndicator(int camera_id, bool is_recording)
{
    auto it = camera_to_widgets_.find(camera_id);
    if (it != camera_to_widgets_.end())
    {
        for (auto *widget : it->second)
        {
            if (widget && widget->isPlaying())
            {
                widget->setRecordingIndicatorFromManager(is_recording);
            }
        }
    }
}

void XViewer::view(int count)
{
    qDebug() << "view" << count;
    int cols     = sqrt(count);
    int wid_size = sizeof(cam_wids) / sizeof(QWidget *);

    auto lay = (QGridLayout *)ui->cams->layout();
    if (!lay)
    {
        lay = new QGridLayout;
        lay->setContentsMargins(0, 0, 0, 0);
        lay->setSpacing(2);
        ui->cams->setLayout(lay);
    }

    for (int i = 0; i < count; i++)
    {
        if (!cam_wids[i])
        {
            cam_wids[i] = new XCameraWidget;
            cam_wids[i]->setStyleSheet("background-color:rgb(51,51,51);");

            // 连接切换视图信号
            connect(cam_wids[i], &XCameraWidget::changeViewMode, this,
                    [this](int mode)
                    {
                        switch (mode)
                        {
                            case 1:
                                View1();
                                break;
                            case 4:
                                View4();
                                break;
                            case 9:
                                View9();
                                break;
                            case 16:
                                View16();
                                break;
                        }
                    });

            // 连接录制状态变化信号，刷新左侧列表
            connect(cam_wids[i], &XCameraWidget::recordingStateChanged, this,
                    [this](int /*cameraId*/, bool /*isRecording*/) { refreshCameras(); });

            // 当摄像头被分配到窗口时，记录映射
            connect(cam_wids[i], &XCameraWidget::cameraAssigned, this,
                    [this, i](int camera_id)
                    {
                        if (camera_id >= 0)
                        {
                            camera_to_widgets_[camera_id].push_back(cam_wids[i]);

                            // 如果这个摄像头正在录制，立即显示 REC
                            if (XRecorderManager::instance().isRecording(camera_id))
                            {
                                cam_wids[i]->setRecordingIndicatorFromManager(true);
                            }
                        }
                    });

            // 当窗口释放摄像头时，清除映射
            connect(cam_wids[i], &XCameraWidget::cameraReleased, this,
                    [this, i](int camera_id)
                    {
                        if (camera_id >= 0)
                        {
                            auto it = camera_to_widgets_.find(camera_id);
                            if (it != camera_to_widgets_.end())
                            {
                                auto &vec = it->second;
                                vec.erase(std::remove(vec.begin(), vec.end(), cam_wids[i]), vec.end());
                                if (vec.empty())
                                {
                                    camera_to_widgets_.erase(it);
                                }
                            }
                        }
                    });
        }
        lay->addWidget(cam_wids[i], i / cols, i % cols);
    }

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
    ui->cam_list->clear();
    int count = c->getCameraCount();
    for (int i = 0; i < count; i++)
    {
        auto    cam  = c->getCamera(i);
        QString text = QString::fromStdString(cam->name);

        // 检查是否有录制
        bool isRecording = XRecorderManager::instance().isRecording(i);
        if (isRecording)
        {
            text += " [录制中]";
        }

        auto item = new QListWidgetItem(QIcon(":/XViewer/img/cam.png"), text);

        if (isRecording)
        {
            item->setForeground(Qt::red);
        }

        ui->cam_list->addItem(item);
    }
}

void XViewer::MaxWindow()
{
    ui->max->setVisible(false);
    ui->normal->setVisible(true);
    showMaximized();
}

void XViewer::NormalWindow()
{
    ui->max->setVisible(true);
    ui->normal->setVisible(false);
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
    updateCam(-1);
}

void XViewer::SetCam()
{
    int row = ui->cam_list->currentIndex().row();
    if (row < 0)
    {
        QMessageBox::information(this, "错误", "请选择摄像机");
        return;
    }
    updateCam(row);
}

void XViewer::DelCam()
{
    int row = ui->cam_list->currentIndex().row();
    if (row < 0)
    {
        QMessageBox::information(this, "错误", "请选择摄像机");
        return;
    }
    std::stringstream ss;
    ss << "您确认需要删除摄像机" << ui->cam_list->currentItem()->text().toLocal8Bit().constData();
    ss << "吗？";

    if (QMessageBox::information(this, "确认", ss.str().c_str(), QMessageBox::Yes, QMessageBox::No) != QMessageBox::Yes)
    {
        return;
    }
    XCameraConfig::instance()->deleteCamera(row);
    XCameraConfig::instance()->save(CAM_CONF_PATH);
    refreshCameras();
}

void XViewer::Preview()
{
    ui->cams->show();
    ui->playback_wid->hide();
    ui->preview->setChecked(true);
}

void XViewer::Playback()
{
    ui->cams->hide();
    ui->playback_wid->show();
    ui->playback->setChecked(true);
}

void XViewer::SelectCamera(QModelIndex index)
{
    int camera_id = index.row();
    qDebug() << "SelectCamera" << camera_id;

    /// 保存当前选中的摄像机ID
    current_selected_camera_ = camera_id;

    /// 获取该摄像机的录像日期列表
    auto dates = XRecorderManager::instance().getRecordDates(camera_id);

    /// 清空并重新设置日历的日期标记
    ui->cal->ClearDate();
    for (const auto &date : dates)
    {
        ui->cal->AddDate(date);
    }

    /// 刷新日历显示（触发重绘，让有录像的日期显示为红色）
    ui->cal->update();

    /// 清空时间列表
    ui->time_list->clear();
}

void XViewer::SelectDate(QDate date)
{
    qDebug() << "SelectDate" << date.toString();
}

void XViewer::PlayVideo(QModelIndex index)
{
    qDebug() << "PlayVideo" << index.row();
}

void XViewer::updateCam(int index)
{
    auto    c = XCameraConfig::instance();
    QDialog dlg(this);
    dlg.resize(800, 200);
    QFormLayout lay;
    dlg.setLayout(&lay);

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
    connect(&save, &QPushButton::clicked, &dlg, &QDialog::accept);
    lay.addRow("", &save);

    /// 编辑时读入原数据显示
    if (index >= 0)
    {
        auto cam = c->getCamera(index);
        name_edit.setText(cam->name);
        url_edit.setText(cam->url);
        sub_url_edit.setText(cam->sub_url);
        save_path_edit.setText(cam->save_path);
    }

    for (;;)
    {
        if (dlg.exec() == QDialog::Accepted)
        {
            if (name_edit.text().isEmpty())
            {
                QMessageBox::information(nullptr, "错误", "请输入名称");
                continue;
            }
            if (url_edit.text().isEmpty())
            {
                QMessageBox::information(nullptr, "错误", "请输入主码流");
                continue;
            }
            if (sub_url_edit.text().isEmpty())
            {
                QMessageBox::information(nullptr, "错误", "请输入辅码流");
                continue;
            }
            if (save_path_edit.text().isEmpty())
            {
                QMessageBox::information(nullptr, "错误", "请输入保存目录");
                continue;
            }
            break;
        }
        return;
    }

    XCameraData data;
    strcpy(data.name, name_edit.text().toStdString().c_str());
    strcpy(data.url, url_edit.text().toStdString().c_str());
    strcpy(data.sub_url, sub_url_edit.text().toStdString().c_str());
    strcpy(data.save_path, save_path_edit.text().toStdString().c_str());

    if (index >= 0)
    {
        c->updateCamera(index, data);
    }
    else
    {
        c->addCamera(data);
    }
    c->save(CAM_CONF_PATH);
    refreshCameras();
}
