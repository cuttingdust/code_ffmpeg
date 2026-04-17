#include "XPlayVideo.h"
#include "ui_xplayvideo.h"

#include <AVLog.h>
#include <LocalPlayer.h>

#include <QtWidgets/QStyle>
#include <QtGui/QCloseEvent>

XPlayVideo::XPlayVideo(QWidget* parent) : QWidget(parent), ui(new Ui::XPlayVideo)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    progress_timer_ = new QTimer(this);
    connect(progress_timer_, &QTimer::timeout, this, &XPlayVideo::updateProgress);

    // 初始化速度下拉框
    ui->speed_combo->clear();
    ui->speed_combo->addItem("0.5x", 0.5);
    ui->speed_combo->addItem("1.0x", 1.0);
    ui->speed_combo->addItem("1.5x", 1.5);
    ui->speed_combo->addItem("2.0x", 2.0);
    ui->speed_combo->addItem("3.0x", 3.0);
    ui->speed_combo->addItem("4.0x", 4.0);
    ui->speed_combo->addItem("5.0x", 5.0);
    ui->speed_combo->setCurrentIndex(1); // 默认 1.0x
}

XPlayVideo::~XPlayVideo()
{
    if (progress_timer_)
    {
        progress_timer_->stop();
    }
    if (player_)
    {
        player_->stop();
    }
    delete ui;
}

void XPlayVideo::adjustWindowSize()
{
    if (video_width_ <= 0 || video_height_ <= 0)
    {
        return;
    }

    int control_height = ui->controlLayout->geometry().height();
    int title_height   = style()->pixelMetric(QStyle::PM_TitleBarHeight);
    int frame_width    = style()->pixelMetric(QStyle::PM_DefaultFrameWidth) * 2;

    int total_width  = video_width_ + frame_width;
    int total_height = video_height_ + control_height + title_height;

    ui->video_widget->setFixedSize(video_width_, video_height_);
    resize(total_width, total_height);

    LOGI("调整窗口大小: " << total_width << "x" << total_height << ", 视频: " << video_width_ << "x" << video_height_);
}

void XPlayVideo::setFile(const std::string& filepath, int camera_id, const QString& camera_name)
{
    filepath_    = filepath;
    camera_id_   = camera_id;
    camera_name_ = camera_name;

    setWindowTitle(QString("回放: %1 - %2").arg(camera_name).arg(QString::fromStdString(filepath)));

    player_ = std::make_unique<LocalPlayer>();

    void* winId = (void*)ui->video_widget->winId();

    if (player_->open(filepath, winId))
    {
        double duration = player_->getDuration();
        int    hours    = int(duration) / 3600;
        int    minutes  = (int(duration) % 3600) / 60;
        int    seconds  = int(duration) % 60;
        ui->total_time_label->setText(QString("%1:%2:%3")
                                              .arg(hours, 2, 10, QChar('0'))
                                              .arg(minutes, 2, 10, QChar('0'))
                                              .arg(seconds, 2, 10, QChar('0')));

        video_width_  = player_->getWidth();
        video_height_ = player_->getHeight();
        adjustWindowSize();

        LOGI("加载文件成功: " << filepath << ", 时长: " << duration << "秒, 分辨率: " << video_width_ << "x"
                              << video_height_);
    }
    else
    {
        LOGE("加载文件失败: " << filepath);
    }
}

void XPlayVideo::play()
{
    if (player_)
    {
        player_->play();
        ui->play_pause_btn->setText("⏸");
        progress_timer_->start(500);
    }
}

void XPlayVideo::stop()
{
    if (player_)
    {
        player_->stop();
    }
    if (progress_timer_)
    {
        progress_timer_->stop();
    }
}

bool XPlayVideo::isPlaying() const
{
    return player_ ? player_->isPlaying() : false;
}

void XPlayVideo::closeEvent(QCloseEvent* event)
{
    stop();
    event->accept();
}

void XPlayVideo::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
}

void XPlayVideo::onPlayPauseClicked()
{
    if (!player_)
        return;

    if (player_->isPlaying())
    {
        if (player_->isPaused())
        {
            player_->resume();
            ui->play_pause_btn->setText("⏸");
            progress_timer_->start();
        }
        else
        {
            player_->pause();
            ui->play_pause_btn->setText("▶");
            progress_timer_->stop();
        }
    }
    else
    {
        player_->play();
        ui->play_pause_btn->setText("⏸");
        progress_timer_->start();
    }
}

void XPlayVideo::onStopClicked()
{
    stop();
    close();
}

// ✅ 修改：按下时只标记，不暂停
void XPlayVideo::onSeekSliderPressed()
{
    is_seeking_ = true;
}

// ✅ 修改：拖动时实时 Seek
void XPlayVideo::onSeekSliderMoved(int value)
{
    if (!player_ || !is_seeking_)
        return;

    double duration  = player_->getDuration();
    double seek_time = duration * value / 1000.0;

    // 实时 Seek
    player_->seek(seek_time);

    // 更新时间标签
    int hours   = int(seek_time) / 3600;
    int minutes = (int(seek_time) % 3600) / 60;
    int seconds = int(seek_time) % 60;
    ui->current_time_label->setText(QString("%1:%2:%3")
                                            .arg(hours, 2, 10, QChar('0'))
                                            .arg(minutes, 2, 10, QChar('0'))
                                            .arg(seconds, 2, 10, QChar('0')));
}

// ✅ 修改：松开时只清除标记
void XPlayVideo::onSeekSliderReleased()
{
    is_seeking_ = false;
}

void XPlayVideo::onSpeedChanged(int index)
{
    if (!player_)
        return;

    double speed = ui->speed_combo->itemData(index).toDouble();
    player_->setSpeed(speed);

    LOGI("播放速度切换到: " << speed << "x");
}

void XPlayVideo::updateProgress()
{
    if (!player_ || is_seeking_)
        return;

    double current  = player_->getCurrentTime();
    double duration = player_->getDuration();

    if (duration > 0)
    {
        int progress = (int)(current / duration * 1000);
        ui->seek_slider->setValue(progress);
    }

    int hours   = int(current) / 3600;
    int minutes = (int(current) % 3600) / 60;
    int seconds = int(current) % 60;
    ui->current_time_label->setText(QString("%1:%2:%3")
                                            .arg(hours, 2, 10, QChar('0'))
                                            .arg(minutes, 2, 10, QChar('0'))
                                            .arg(seconds, 2, 10, QChar('0')));

    if (current >= duration - 0.1 && duration > 0)
    {
        progress_timer_->stop();
        ui->play_pause_btn->setText("▶");
    }
}
