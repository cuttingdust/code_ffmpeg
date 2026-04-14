#pragma once

#include <QtWidgets/QWidget>
#include <QtWidgets/QMenu>
#include <QtCore/QModelIndex>
#include <QtCore/QDate>
#include <memory>
#include <unordered_map>

namespace Ui
{
    class XViewerClass;
}

class XCameraWidget;

class XViewer : public QWidget
{
    Q_OBJECT

public:
    explicit XViewer(QWidget *parent = nullptr);
    ~XViewer();

protected:
    bool eventFilter(QObject *pObj, QEvent *pEvent) override;
    void resizeEvent(QResizeEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private slots:
    void View1();
    void View4();
    void View9();
    void View16();
    void MaxWindow();
    void NormalWindow();
    void AddCam();
    void SetCam();
    void DelCam();

    void Preview();  /// 预览界面
    void Playback(); /// 回放界面

    void SelectCamera(QModelIndex index); /// 选择摄像机
    void SelectDate(QDate date);          /// 选择日期
    void PlayVideo(QModelIndex index);    /// 选择时间播放视频

private:
    void view(int count);
    void refreshCameras();
    void updateCam(int index);

    /// 处理录制状态变化
    void onRecordingStatusChanged(int camera_id, bool is_recording);

    /// 更新指定摄像头的所有窗口的 REC 显示
    void updateCameraRecIndicator(int camera_id, bool is_recording);

private:
    Ui::XViewerClass *ui;
    QMenu             left_menu_;

    // 记录每个 camera_id 对应的播放窗口（一个摄像头可能被多个窗口播放）
    std::unordered_map<int, std::vector<XCameraWidget *>> camera_to_widgets_;
    int                                                   current_selected_camera_ = -1; // 当前选中的摄像机ID
};
