#include "XCameraConfig.h"
#include "XViewer.h"

#include <AVLog.h>
#include <AVLogQt.h>

#include <QtWidgets/QApplication>

#include <iostream>

#define TEST_CAM_PATH "test.db"

int main(int argc, char *argv[])
{
    setlocale(LC_ALL, "zh_CN.UTF-8");

    /// Qt 分类过滤（在 QApplication 之前设置）
    qputenv("QT_LOGGING_RULES",
            "qt.gui.imageio=false\n"
            "qt.css.*=false");

    QApplication a(argc, argv);

    avLogInit();
    avLogInstallQtMessageHandler();

    // auto *xc = XCameraConfig::instance();
    // xc->load(TEST_CAM_PATH);
    // {
    //     XCameraData cd;
    //     strcpy(cd.name, "camera1");
    //     strcpy(cd.save_path, ".\\camera1\\");
    //     strcpy(cd.url, "rtsp://test:x12345678@192.168.2.64/h264/ch1/main/av_stream");
    //     strcpy(cd.sub_url, "rtsp://test:x12345678@192.168.2.64/h264/ch1/sub/av_stream");
    //     xc->addCamera(cd);
    // }
    //
    // {
    //     XCameraData cd;
    //     strcpy(cd.name, "camera2");
    //     strcpy(cd.save_path, ".\\camera2\\");
    //     strcpy(cd.url, "rtsp://admin:admin@192.168.2.108/cam/realmonitor?channel=1&subtype=0");
    //     strcpy(cd.sub_url, "rtsp://admin:admin@192.168.2.108/cam/realmonitor?channel=1&subtype=1");
    //     xc->addCamera(cd);
    // }
    //
    // int cam_count = xc->getCameraCount();
    // for (int i = 0; i < cam_count; i++)
    // {
    //     if (auto cam = xc->getCamera(i))
    //     {
    //         qDebug() << cam->name;
    //     }
    //     else
    //     {
    //         qDebug() << "Camera not found at index" << i;
    //     }
    // }
    // qDebug() << "=================Set=====================";
    // auto d1 = xc->getCamera(0);
    // strcpy(d1->name, "camera_001");
    // xc->updateCamera(0, d1.value());
    // cam_count = xc->getCameraCount();
    // for (int i = 0; i < cam_count; i++)
    // {
    //     if (auto cam = xc->getCamera(i))
    //     {
    //         qDebug() << cam->name;
    //     }
    //     else
    //     {
    //         qDebug() << "Camera not found at index" << i;
    //     }
    // }
    // xc->save(TEST_CAM_PATH);
    //
    // qDebug() << "=================Del=====================";
    // xc->deleteCamera(1);
    // cam_count = xc->getCameraCount();
    // for (int i = 0; i < cam_count; i++)
    // {
    //     if (auto cam = xc->getCamera(i))
    //     {
    //         qDebug() << cam->name;
    //     }
    //     else
    //     {
    //         qDebug() << "Camera not found at index" << i;
    //     }
    // }
    // xc->deleteCamera(0);

    XViewer w;
    w.show();
    return a.exec();
}
