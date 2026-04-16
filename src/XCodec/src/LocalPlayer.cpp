#include "LocalPlayer.h"
#include "AVLog.h"
#include <chrono>

LocalPlayer::LocalPlayer()
{
    LOGI("本地播放器创建");
}

LocalPlayer::~LocalPlayer()
{
    stop();
}

bool LocalPlayer::open(const std::string& filepath, void* winId)
{
    filepath_ = filepath;
    window_   = winId;

    try
    {
        demux_task_   = XDemuxTask::create();
        decode_task_  = XDecodeTask::create();
        display_task_ = XDisplayTask::create();

        demux_task_->setName("LocalDemux");
        decode_task_->setName("LocalDecode");
        display_task_->setName("LocalDisplay");

        demux_task_->setNext(decode_task_);
        decode_task_->setNext(display_task_);

        /// 设置队列大小
        demux_task_->setMaxQueueSize(500);
        decode_task_->setMaxQueueSize(500);
        display_task_->setMaxQueueSize(1000);

        /// 设置空闲超时（本地文件不需要超时检测）
        demux_task_->setIdleTimeoutMs(0);
        decode_task_->setIdleTimeoutMs(0);
        display_task_->setIdleTimeoutMs(0);

        if (!demux_task_->open(filepath))
        {
            LOGE("打开文件失败: " << filepath);
            return false;
        }

        auto video_stream = demux_task_->getVideoStream();
        if (!video_stream)
        {
            LOGE("未找到视频流");
            return false;
        }

        video_width_  = video_stream->codecpar->width;
        video_height_ = video_stream->codecpar->height;
        duration_     = demux_task_->getDuration();

        if (video_stream->avg_frame_rate.num > 0)
        {
            frame_rate_ = av_q2d(video_stream->avg_frame_rate);
        }
        else if (video_stream->r_frame_rate.num > 0)
        {
            frame_rate_ = av_q2d(video_stream->r_frame_rate);
        }

        LOGI("视频: " << video_width_ << "x" << video_height_ << ", 时长: " << duration_ << "秒"
                      << ", 帧率: " << frame_rate_ << " fps");

        decode_task_->setHardwareDecode(false);
        if (!decode_task_->initDecoder(video_stream->codecpar->codec_id, video_stream))
        {
            LOGE("初始化解码器失败");
            return false;
        }

        display_task_->setWindow(winId);

        auto error_cb = [this](const std::string& msg) { LOGE("LocalPlayer 错误: " << msg); };
        demux_task_->setErrorCallback(error_cb);
        decode_task_->setErrorCallback(error_cb);
        display_task_->setErrorCallback(error_cb);

        LOGI("打开文件成功: " << filepath);
        return true;
    }
    catch (const std::exception& e)
    {
        LOGE("打开文件异常: " << e.what());
        return false;
    }
}

void LocalPlayer::play()
{
    if (is_playing_)
    {
        LOGW("已经在播放中");
        return;
    }

    should_stop_  = false;
    is_playing_   = true;
    is_paused_    = false;
    is_finished_  = false;
    seek_request_ = false;

    // 启动控制线程
    control_thread_ = std::thread(&LocalPlayer::controlLoop, this);

    // 启动任务链（先启动下游，再启动上游）
    display_task_->start();
    decode_task_->start();
    demux_task_->start();

    LOGI("开始播放: " << filepath_);
}

void LocalPlayer::pause()
{
    if (!is_playing_ || is_paused_)
    {
        return;
    }
    is_paused_ = true;

    // ✅ 通知解封装任务暂停
    if (demux_task_)
    {
        demux_task_->setPaused(true);
    }

    if (decode_task_)
    {
        decode_task_->setPaused(true);
    }

    if (display_task_)
    {
        display_task_->setPaused(true);
    }

    LOGI("暂停播放");
}

void LocalPlayer::resume()
{
    if (!is_playing_ || !is_paused_)
    {
        return;
    }

    // ✅ 通知解封装任务恢复
    if (demux_task_)
    {
        demux_task_->setPaused(false);
    }

    if (decode_task_)
    {
        decode_task_->setPaused(false);
    }

    if (display_task_)
    {
        display_task_->setPaused(false);
    }

    is_paused_ = false;
    LOGI("恢复播放");
}

void LocalPlayer::stop()
{
    if (!is_playing_ && !is_finished_)
    {
        return;
    }

    LOGI("停止播放");
    should_stop_ = true;
    is_playing_  = false;
    is_paused_   = false;

    if (demux_task_)
    {
        demux_task_->stop();
        decode_task_->stop();
        display_task_->stop();
    }

    if (control_thread_.joinable())
    {
        control_thread_.join();
    }

    if (demux_task_)
    {
        demux_task_->wait();
        decode_task_->wait();
        display_task_->wait();
    }

    if (demux_task_)
    {
        demux_task_->reset();
        decode_task_->reset();
        display_task_->reset();
    }

    LOGI("播放已停止");
}

void LocalPlayer::seek(double seconds)
{
    if (!demux_task_)
    {
        return;
    }

    seek_target_  = seconds;
    seek_request_ = true;
    LOGI("请求跳转到: " << seconds << "秒");
}

std::map<PlaybackSpeed, double> LocalPlayer::getSupportedSpeeds()
{
    static const std::map<PlaybackSpeed, double> speeds = {
        { PlaybackSpeed::SPEED_0_5X, 0.5 }, { PlaybackSpeed::SPEED_1_0X, 1.0 }, { PlaybackSpeed::SPEED_1_5X, 1.5 },
        { PlaybackSpeed::SPEED_2_0X, 2.0 }, { PlaybackSpeed::SPEED_3_0X, 3.0 }, { PlaybackSpeed::SPEED_4_0X, 4.0 },
        { PlaybackSpeed::SPEED_5_0X, 5.0 }
    };
    return speeds;
}

void LocalPlayer::setSpeed(PlaybackSpeed speed)
{
    auto speeds = getSupportedSpeeds();
    auto it     = speeds.find(speed);
    if (it != speeds.end())
    {
        setSpeed(it->second);
    }
    else
    {
        LOGW("无效的播放速度枚举");
    }
}

void LocalPlayer::setSpeed(double speed)
{
    if (speed <= 0 || speed > 10.0)
    {
        LOGW("无效的播放速度: " << speed);
        return;
    }

    speed_ = speed;

    if (demux_task_)
    {
        demux_task_->setSpeed(speed);
    }

    LOGI("设置播放速度: " << speed << "x");
}

double LocalPlayer::getDuration() const
{
    return duration_;
}

double LocalPlayer::getCurrentTime() const
{
    // TODO: 从 demux_task 获取当前 PTS
    return 0.0;
}

void LocalPlayer::controlLoop()
{
    LOGI("控制线程启动");

    while (!should_stop_ && is_playing_)
    {
        // 处理暂停
        if (is_paused_)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        // 检查是否播放结束
        if (demux_task_ && demux_task_->isEofReached())
        {
            LOGI("文件读取完成，等待队列清空...");

            // 等待下游队列清空
            for (int i = 0; i < 10; i++)
            {
                if ((!decode_task_ || decode_task_->getQueueSize() == 0) &&
                    (!display_task_ || display_task_->getQueueSize() == 0))
                {
                    LOGI("播放结束");
                    is_playing_  = false;
                    is_finished_ = true;
                    return;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }

            LOGI("等待超时，强制结束");
            is_playing_  = false;
            is_finished_ = true;
            return;
        }

        // 处理跳转请求
        if (seek_request_)
        {
            double target = seek_target_;
            seek_request_ = false;

            LOGI("执行跳转到: " << target << "秒");

            bool was_paused = is_paused_;
            if (!was_paused)
            {
                demux_task_->stop();
                decode_task_->stop();
                display_task_->stop();

                demux_task_->wait();
                decode_task_->wait();
                display_task_->wait();
            }

            if (demux_task_->seek(target))
            {
                decode_task_->reset();
                LOGI("跳转成功");
            }
            else
            {
                LOGE("跳转失败");
            }

            if (!was_paused && !should_stop_)
            {
                display_task_->start();
                decode_task_->start();
                demux_task_->start();
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    LOGI("控制线程结束");
}
