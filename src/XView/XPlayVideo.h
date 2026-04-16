#pragma once

#include <QtWidgets/QWidget>
#include <QtCore/QTimer>
#include <memory>
#include <string>
#include <map>

class LocalPlayer;

namespace Ui
{
    class XPlayVideo;
}

class XPlayVideo : public QWidget
{
    Q_OBJECT

public:
    explicit XPlayVideo(QWidget *parent = nullptr);
    ~XPlayVideo();

    void setFile(const std::string &filepath, int camera_id, const QString &camera_name);
    void play();
    void stop();
    bool isPlaying() const;

protected:
    void closeEvent(QCloseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void onPlayPauseClicked();
    void onStopClicked();
    void onSeekSliderPressed();
    void onSeekSliderReleased();
    void onSeekSliderMoved(int value);
    void onSpeedChanged(int index);
    void updateProgress();

private:
    void adjustWindowSize();
    void updateSpeedButton(int index); // ✅ 更新速度按钮状态

private:
    Ui::XPlayVideo *ui;

    std::unique_ptr<LocalPlayer> player_;
    std::string                  filepath_;
    int                          camera_id_ = -1;
    QString                      camera_name_;
    int                          video_width_  = 0;
    int                          video_height_ = 0;

    QTimer *progress_timer_ = nullptr;
    bool    is_seeking_     = false;

    // ✅ 速度映射表
    std::map<int, double> speed_map_;
};
