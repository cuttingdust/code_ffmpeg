#pragma once

#include <QtWidgets/QWidget>
#include <QtCore/QMargins>
#include <QtCore/QTimer>
#include <memory>
#include <string>

class LocalPlayer;
class QKeyEvent;

namespace Ui
{
    class XPlayVideo;
}

class XPlayVideo : public QWidget
{
    Q_OBJECT

public:
    explicit XPlayVideo(QWidget *parent = nullptr);
    ~XPlayVideo() override;

    void setFile(const std::string &filepath, int camera_id, const QString &camera_name);
    void play();
    void stop();
    bool isPlaying() const;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void onPlayPauseClicked();
    void onStopClicked();
    void onSeekSliderPressed();
    void onSeekSliderReleased();
    void onSeekSliderMoved(int value);
    void onSpeedChanged(int index);
    void onVolumeChanged(int value);
    void updateProgress();

private:
    void adjustWindowSize();
    void adjustVolume(int offset);
    void installShortcuts();
    void seekBySeconds(double offset_seconds);
    void seekToTime(double seconds);
    void seekToSliderValue(int value, bool resume_after_seek);
    void setControlBarVisible(bool visible);
    void toggleMute();
    void toggleFullScreen();
    void updateResponsiveControls(int window_width);

private:
    Ui::XPlayVideo *ui;

    std::unique_ptr<LocalPlayer> player_;
    std::string                  filepath_;
    int                          camera_id_ = -1;
    QString                      camera_name_;
    int                          video_width_  = 0;
    int                          video_height_ = 0;
    Qt::WindowStates             normal_window_state_ = Qt::WindowNoState;
    QMargins                     control_layout_margins_;
    int                          volume_before_mute_ = 100;

    QTimer *progress_timer_ = nullptr;
    bool    is_seeking_     = false;
    bool    resume_after_seek_ = false;

    // 防抖相关
    QTimer *seek_timer_         = nullptr;
    int     pending_seek_value_ = -1;
};
