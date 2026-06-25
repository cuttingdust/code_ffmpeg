#include "XPlayVideo.h"
#include "ui_xplayvideo.h"

#include <AVLog.h>
#include <LocalPlayer.h>
#include <XOpenGLVideoWidget.h>
#include <XOverlayUtil.h>

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

    ui->volume_slider->setRange(0, 100);
    ui->volume_slider->setValue(100);
    ui->volume_slider->setEnabled(false);

    ui->controlLayout->setStretch(ui->controlLayout->indexOf(ui->seek_slider), 1);

    // 防抖定时器
    seek_timer_ = new QTimer(this);
    seek_timer_->setSingleShot(true);
    connect(seek_timer_, &QTimer::timeout, this,
            [this]()
            {
                if (pending_seek_value_ >= 0 && player_)
                {
                    double duration  = player_->getDuration();
                    double seek_time = duration * pending_seek_value_ / 1000.0;
                    player_->seek(seek_time);
                    LOGI("防抖 Seek 执行: " << seek_time << "秒");
                    pending_seek_value_ = -1;
                }
            });
}

XPlayVideo::~XPlayVideo()
{
    if (progress_timer_)
    {
        progress_timer_->stop();
    }
    if (seek_timer_)
    {
        seek_timer_->stop();
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

    ui->openGLWidget->setFixedSize(video_width_, video_height_);
    resize(total_width, total_height);

    LOGI("调整窗口大小: " << total_width << "x" << total_height << ", 视频: " << video_width_ << "x" << video_height_);
}

void XPlayVideo::setFile(const std::string& filepath, int camera_id, const QString& camera_name)
{
    filepath_    = filepath;
    camera_id_   = camera_id;
    camera_name_ = camera_name;

    setWindowTitle(QString("回放: %1 - %2").arg(camera_name).arg(QString::fromStdString(filepath)));

    if (player_)
    {
        player_->stop();
    }

    player_ = std::make_unique<LocalPlayer>();
    player_->setOpenGLWidget(ui->openGLWidget);
    player_->setRenderBackend(RenderBackend::OpenGL);
    player_->setOverlayStyle(defaultRecOverlayStyle());

    if (player_->open(filepath))
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

        ui->volume_slider->setEnabled(player_->hasAudio());
        if (player_->hasAudio())
        {
            ui->volume_slider->setValue(static_cast<int>(player_->getVolume() * 100));
            ui->volume_label->setText(QStringLiteral("🔊"));
            ui->volume_label->setToolTip(tr("音量"));
        }
        else
        {
            ui->volume_label->setText(QStringLiteral("🔇"));
            ui->volume_label->setToolTip(tr("该录像无音频（当前录制仅保存视频流）"));
        }

        LOGI("加载文件成功: " << filepath << ", 时长: " << duration << "秒, 分辨率: " << video_width_ << "x"
                              << video_height_ << ", 音频: " << (player_->hasAudio() ? "有" : "无"));
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

void XPlayVideo::onSeekSliderPressed()
{
    is_seeking_ = true;

    if (player_ && player_->isPlaying() && !player_->isPaused())
    {
        resume_after_seek_ = true;
        player_->pause();
        ui->play_pause_btn->setText("▶");
        progress_timer_->stop();
    }
    else
    {
        resume_after_seek_ = false;
    }
}

void XPlayVideo::onSeekSliderMoved(int value)
{
    if (!player_ || !is_seeking_)
        return;

    double duration  = player_->getDuration();
    double seek_time = duration * value / 1000.0;

    // 更新时间标签显示（实时）
    int hours   = int(seek_time) / 3600;
    int minutes = (int(seek_time) % 3600) / 60;
    int seconds = int(seek_time) % 60;
    ui->current_time_label->setText(QString("%1:%2:%3")
                                            .arg(hours, 2, 10, QChar('0'))
                                            .arg(minutes, 2, 10, QChar('0'))
                                            .arg(seconds, 2, 10, QChar('0')));

    // 防抖：记录最终位置，延迟执行 Seek
    pending_seek_value_ = value;
    seek_timer_->start(150);
}

void XPlayVideo::onSeekSliderReleased()
{
    is_seeking_ = false;

    // 确保最后一次 Seek 被执行
    if (pending_seek_value_ >= 0 && player_)
    {
        seek_timer_->stop();
        double duration  = player_->getDuration();
        double seek_time = duration * pending_seek_value_ / 1000.0;
        player_->seek(seek_time);
        LOGI("Seek 完成，位置: " << seek_time << "秒");
        pending_seek_value_ = -1;
    }

    if (resume_after_seek_ && player_)
    {
        player_->resume();
        ui->play_pause_btn->setText("⏸");
        progress_timer_->start(500);
    }
    resume_after_seek_ = false;
}

void XPlayVideo::onSpeedChanged(int index)
{
    if (!player_)
        return;

    double speed = ui->speed_combo->itemData(index).toDouble();
    player_->setSpeed(speed);

    LOGI("播放速度切换到: " << speed << "x");
}

void XPlayVideo::onVolumeChanged(int value)
{
    if (!player_ || !player_->hasAudio())
        return;

    player_->setVolume(value / 100.0);
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
