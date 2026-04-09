#pragma once

#include <QtWidgets/QWidget>
#include <memory>

class RtspClient;

class XCameraWidget : public QWidget
{
    Q_OBJECT
public:
    XCameraWidget(QWidget *p = nullptr);
    ~XCameraWidget() override;

    /// 打开RTSP流
    auto open(const QString &url) -> bool;

    /// 关闭播放
    auto close() const -> void;

    auto setCameraId(int id) -> void;

    auto getCameraId() const -> int;

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    class PImpl;
    std::unique_ptr<PImpl> impl_;
};
