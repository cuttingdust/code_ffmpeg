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
class XPlayVideo;

class XViewer : public QWidget
{
    Q_OBJECT

public:
    explicit XViewer(QWidget *parent = nullptr);
    ~XViewer() override;

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

    void SelectCamera(QModelIndex index); /// 选择摄像机（回放界面用）
    void SelectDate(QDate date);          /// 选择日期
    void PlayVideo(QModelIndex index);    /// 选择时间播放视频

private:
    void view(int count);
    void refreshCameras();
    void updateCam(int index);
    void refreshPlaybackDates(); /// 刷新回放界面的日期显示

    /// 处理录制状态变化
    void onRecordingStatusChanged(int camera_id, bool is_recording);

    /// 更新指定摄像头的所有窗口的 REC 显示
    void updateCameraRecIndicator(int camera_id, bool is_recording);

    /// 进入回放前暂停所有预览音频（不断 RTSP 视频）
    void suspendAllPreviews();

    /// 回到预览后恢复曾开启声音的窗口
    void resumeAllPreviews();

    /// 关闭当前回放窗口，避免与预览/新回放争用音频
    void closeActivePlayback();

    /// 从 camera_to_widgets_ 移除窗口映射
    void removeCameraWidgetMapping(XCameraWidget *widget, int camera_id);

    /// 关闭所有预览窗口并断开信号（析构/切视图前）
    void shutdownPreviewWidgets();

private:
    Ui::XViewerClass *ui;
    QMenu             left_menu_;

    /// 记录每个 camera_id 对应的播放窗口（预览界面）
    std::unordered_map<int, std::vector<XCameraWidget *>> camera_to_widgets_;

    /// 预览界面：当前正在播放的摄像机（拖拽到窗口的）
    int preview_playing_camera_ = -1;

    /// 回放界面：当前选中的摄像机（点击左侧列表选中的）
    int playback_selected_camera_ = -1;

    /// 当前打开的回放窗口（同时只允许一个）
    XPlayVideo *active_playback_ = nullptr;

    bool shutting_down_ = false;
};
