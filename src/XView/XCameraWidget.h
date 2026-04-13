#pragma once

#include <QtWidgets/QWidget>
#include <memory>
#include <atomic>

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
    auto stop() -> void;

    /// 是否正在播放
    auto isPlaying() const -> bool;

    /// 开始录制（主码流）
    auto startRecording() -> void;

    /// 停止录制
    auto stopRecording() -> void;

    /// 是否正在录制
    auto isRecording() const -> bool;

    /// 设置摄像机ID（列表中的索引）
    auto setCameraId(int id) -> void;

    /// 获取摄像机ID
    auto getCameraId() const -> int;

    /// 获取摄像机名称
    auto getCameraName() const -> QString;

signals:
    /// 切换视图信号
    void changeViewMode(int count);

    /// 录制状态变化信号
    void recordingStateChanged(int cameraId, bool isRecording);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    void updateMenuState();

private:
    class PImpl;
    std::unique_ptr<PImpl> impl_;

    QMenu   *context_menu_        = nullptr;
    QAction *start_record_action_ = nullptr;
    QAction *stop_record_action_  = nullptr;

    QTimer           *rec_timer_ = nullptr;        ///< 录制标识刷新定时器
    std::atomic<bool> is_recording_flag_{ false }; ///< 录制状态标志（用于UI显示）
};
