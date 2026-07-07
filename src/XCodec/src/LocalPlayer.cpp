#include "LocalPlayer.h"
#include "AVLog.h"
#include "XOpenGLDisplay.h"
#include "XRecordingOverlay.h"
#include "XOverlayUtil.h"
#include "XOpenGLVideoWidget.h"
#include "XDemuxTask.h"
#include "XVideoDecodeTask.h"
#include "XVideoDisplayTask.h"
#include "XAudioDecodeTask.h"
#include "XAudioPlayTask.h"

#include <atomic>
#include <chrono>
#include <thread>

class LocalPlayer::PImpl
{
public:
    void controlLoop();

    auto setAllTasksPaused(bool paused) -> void
    {
        if (demux_task_)
        {
            demux_task_->setPaused(paused);
        }
        if (decode_task_)
        {
            decode_task_->setPaused(paused);
        }
        if (display_task_)
        {
            display_task_->setPaused(paused);
        }
        if (audio_decode_task_)
        {
            audio_decode_task_->setPaused(paused);
        }
        if (audio_play_task_)
        {
            audio_play_task_->setPaused(paused);
        }
    }

public:
    XDemuxTask::Ptr        demux_task_;
    XVideoDecodeTask::Ptr  decode_task_;
    XVideoDisplayTask::Ptr display_task_;
    XAudioDecodeTask::Ptr  audio_decode_task_;
    XAudioPlayTask::Ptr    audio_play_task_;

    std::string         filepath_;
    void*               window_         = nullptr;
    XOpenGLVideoWidget* opengl_widget_  = nullptr;
    RenderBackend       render_backend_ = RenderBackend::SDL;
    XOverlayStyle       overlay_style_;
    double              duration_     = 0.0;
    double              frame_rate_   = 25.0;
    int                 video_width_  = 0;
    int                 video_height_ = 0;

    std::atomic<bool>   is_playing_{ false };
    std::atomic<bool>   is_paused_{ false };
    std::atomic<bool>   is_finished_{ false };
    std::atomic<bool>   should_stop_{ false };
    std::atomic<bool>   seek_request_{ false };
    std::atomic<double> seek_target_{ 0.0 };
    std::atomic<double> speed_{ 1.0 };

    std::thread control_thread_;
};

LocalPlayer::LocalPlayer() : impl_(std::make_unique<PImpl>())
{
    impl_->overlay_style_ = defaultRecOverlayStyle();
    LOGI("本地播放器创建");
}

LocalPlayer::~LocalPlayer()
{
    stop();
}

void LocalPlayer::setOpenGLWidget(QWidget* widget)
{
    impl_->opengl_widget_ = dynamic_cast<XOpenGLVideoWidget*>(widget);
}

void LocalPlayer::setRenderBackend(RenderBackend backend)
{
    impl_->render_backend_ = backend;
}

RenderBackend LocalPlayer::renderBackend() const
{
    return impl_->render_backend_;
}

void LocalPlayer::setOverlayStyle(const XOverlayStyle& style)
{
    impl_->overlay_style_ = style;
    applyOverlayStyle(style, impl_->display_task_.get(), impl_->opengl_widget_);
}

bool LocalPlayer::open(const std::string& filepath, void* winId)
{
    impl_->filepath_ = filepath;
    impl_->window_   = winId;

    try
    {
        impl_->demux_task_ = XDemuxTask::create();
        impl_->demux_task_->setName("LocalDemux");
        impl_->demux_task_->setMaxQueueSize(500);
        impl_->demux_task_->setIdleTimeoutMs(0);

        if (!impl_->demux_task_->open(filepath))
        {
            LOGE("打开文件失败: " << filepath);
            return false;
        }

        auto* video_stream = impl_->demux_task_->getVideoStream();
        auto* audio_stream = impl_->demux_task_->getAudioStream();
        if (!video_stream && !audio_stream)
        {
            LOGE("未找到音视频流");
            return false;
        }

        impl_->duration_ = impl_->demux_task_->getDuration();

        if (video_stream)
        {
            impl_->decode_task_  = XVideoDecodeTask::create();
            impl_->display_task_ = XVideoDisplayTask::create();

            impl_->decode_task_->setName("LocalDecode");
            impl_->display_task_->setName("LocalDisplay");

            impl_->demux_task_->setNext(impl_->decode_task_);
            impl_->decode_task_->setNext(impl_->display_task_);

            impl_->decode_task_->setMaxQueueSize(500);
            impl_->display_task_->setMaxQueueSize(1000);
            impl_->decode_task_->setIdleTimeoutMs(0);
            impl_->display_task_->setIdleTimeoutMs(0);

            impl_->video_width_  = video_stream->codecpar->width;
            impl_->video_height_ = video_stream->codecpar->height;

            if (video_stream->avg_frame_rate.num > 0)
            {
                impl_->frame_rate_ = av_q2d(video_stream->avg_frame_rate);
            }
            else if (video_stream->r_frame_rate.num > 0)
            {
                impl_->frame_rate_ = av_q2d(video_stream->r_frame_rate);
            }

            LOGI("视频: " << impl_->video_width_ << "x" << impl_->video_height_ << ", 帧率: " << impl_->frame_rate_
                          << " fps");

            impl_->decode_task_->setHardwareDecode(false);
            if (!impl_->decode_task_->initDecoder(video_stream->codecpar->codec_id, video_stream))
            {
                LOGE("初始化解码器失败");
                return false;
            }

            if (impl_->render_backend_ == RenderBackend::OpenGL)
            {
                if (!impl_->opengl_widget_)
                {
                    LOGE("OpenGL 渲染需要先调用 setOpenGLWidget()");
                    return false;
                }

                bindOpenGLDisplayTask(impl_->display_task_.get(), impl_->opengl_widget_);
                impl_->opengl_widget_->setOverlayStyle(impl_->overlay_style_);
                LOGI("使用 OpenGL 主线程渲染");
            }
            else
            {
                bindSdlDisplayTask(impl_->display_task_.get(), winId);
                LOGI("使用 SDL 渲染");
            }
        }
        else
        {
            LOGI("无视频流，仅音频播放");
        }

        if (audio_stream)
        {
            impl_->audio_decode_task_ = XAudioDecodeTask::create();
            impl_->audio_play_task_   = XAudioPlayTask::create();

            impl_->audio_decode_task_->setName("LocalAudioDecode");
            impl_->audio_play_task_->setName("LocalAudioPlay");

            impl_->demux_task_->setAudioNext(impl_->audio_decode_task_);
            impl_->audio_decode_task_->setNext(impl_->audio_play_task_);

            impl_->audio_decode_task_->setMaxQueueSize(500);
            impl_->audio_play_task_->setMaxQueueSize(200);
            impl_->audio_decode_task_->setIdleTimeoutMs(0);
            impl_->audio_play_task_->setIdleTimeoutMs(0);

            LOGI("音频: " << audio_stream->codecpar->sample_rate << "Hz");

            if (!impl_->audio_decode_task_->initDecoder(audio_stream))
            {
                LOGE("音频解码器初始化失败");
                return false;
            }

            if (!impl_->audio_play_task_->openFromDecoder(impl_->audio_decode_task_->getDecoder()))
            {
                LOGE("音频播放设备打开失败");
                return false;
            }

            impl_->audio_play_task_->setVolume(1.0);
            impl_->audio_decode_task_->setSpeed(impl_->speed_.load());
            impl_->audio_play_task_->setSpeed(impl_->speed_.load());
        }

        LOGI("时长: " << impl_->duration_ << " 秒");

        auto error_cb = [](const std::string& msg) { LOGE("LocalPlayer 错误: " << msg); };
        impl_->demux_task_->setErrorCallback(error_cb);
        if (impl_->decode_task_)
        {
            impl_->decode_task_->setErrorCallback(error_cb);
        }
        if (impl_->display_task_)
        {
            impl_->display_task_->setErrorCallback(error_cb);
        }
        if (impl_->audio_decode_task_)
        {
            impl_->audio_decode_task_->setErrorCallback(error_cb);
        }
        if (impl_->audio_play_task_)
        {
            impl_->audio_play_task_->setErrorCallback(error_cb);
        }

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
    if (impl_->is_playing_)
    {
        LOGW("已经在播放中");
        return;
    }

    impl_->should_stop_  = false;
    impl_->is_playing_   = true;
    impl_->is_paused_    = false;
    impl_->is_finished_  = false;
    impl_->seek_request_ = false;

    impl_->control_thread_ = std::thread(&LocalPlayer::PImpl::controlLoop, impl_.get());

    if (impl_->audio_play_task_)
    {
        impl_->audio_play_task_->start();
    }
    if (impl_->audio_decode_task_)
    {
        impl_->audio_decode_task_->start();
    }
    if (impl_->display_task_)
    {
        impl_->display_task_->start();
    }
    if (impl_->decode_task_)
    {
        impl_->decode_task_->start();
    }
    impl_->demux_task_->start();

    LOGI("开始播放: " << impl_->filepath_);
}

void LocalPlayer::pause()
{
    if (!impl_->is_playing_ || impl_->is_paused_)
    {
        return;
    }
    impl_->is_paused_ = true;
    impl_->setAllTasksPaused(true);

    LOGI("暂停播放");
}

void LocalPlayer::resume()
{
    if (!impl_->is_playing_ || !impl_->is_paused_)
    {
        return;
    }

    impl_->setAllTasksPaused(false);
    impl_->is_paused_ = false;
    LOGI("恢复播放");
}

void LocalPlayer::stop()
{
    if (!impl_->demux_task_)
    {
        return;
    }

    if (!impl_->is_playing_ && !impl_->is_finished_)
    {
        LOGI("释放已打开但未播放的 pipeline");
        impl_->demux_task_->reset();
        if (impl_->decode_task_)
        {
            impl_->decode_task_->reset();
        }
        if (impl_->display_task_)
        {
            impl_->display_task_->reset();
        }
        if (impl_->audio_decode_task_)
        {
            impl_->audio_decode_task_->reset();
        }
        if (impl_->audio_play_task_)
        {
            impl_->audio_play_task_->reset();
        }

        impl_->decode_task_.reset();
        impl_->display_task_.reset();
        impl_->audio_decode_task_.reset();
        impl_->audio_play_task_.reset();
        impl_->demux_task_.reset();
        LOGI("pipeline 已释放");
        return;
    }

    LOGI("停止播放");
    impl_->should_stop_ = true;
    impl_->is_playing_  = false;
    impl_->is_paused_   = false;

    if (impl_->demux_task_)
    {
        impl_->demux_task_->stop();
        if (impl_->decode_task_)
        {
            impl_->decode_task_->stop();
        }
        if (impl_->display_task_)
        {
            impl_->display_task_->stop();
        }
        if (impl_->audio_decode_task_)
        {
            impl_->audio_decode_task_->stop();
        }
        if (impl_->audio_play_task_)
        {
            impl_->audio_play_task_->stop();
        }
    }

    if (impl_->control_thread_.joinable())
    {
        impl_->control_thread_.join();
    }

    if (impl_->demux_task_)
    {
        impl_->demux_task_->wait();
        if (impl_->decode_task_)
        {
            impl_->decode_task_->wait();
        }
        if (impl_->display_task_)
        {
            impl_->display_task_->wait();
        }
        if (impl_->audio_decode_task_)
        {
            impl_->audio_decode_task_->wait();
        }
        if (impl_->audio_play_task_)
        {
            impl_->audio_play_task_->wait();
        }
    }

    if (impl_->demux_task_)
    {
        impl_->demux_task_->reset();
        if (impl_->decode_task_)
        {
            impl_->decode_task_->reset();
        }
        if (impl_->display_task_)
        {
            impl_->display_task_->reset();
        }
        if (impl_->audio_decode_task_)
        {
            impl_->audio_decode_task_->reset();
        }
        if (impl_->audio_play_task_)
        {
            impl_->audio_play_task_->reset();
        }

        impl_->decode_task_.reset();
        impl_->display_task_.reset();
        impl_->audio_decode_task_.reset();
        impl_->audio_play_task_.reset();
        impl_->demux_task_.reset();
    }

    LOGI("播放已停止");
}

void LocalPlayer::seek(double seconds)
{
    if (!impl_->demux_task_)
    {
        return;
    }

    /// Seek 期间暂停整条 pipeline（含 SDL 音频），避免拖拽时音画不同步
    const bool was_user_paused = impl_->is_paused_.load();
    impl_->setAllTasksPaused(true);

    impl_->demux_task_->seek(seconds);

    if (!was_user_paused)
    {
        impl_->setAllTasksPaused(false);
    }

    LOGI("Seek 到: " << seconds << "秒");
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

    impl_->speed_ = speed;

    if (impl_->demux_task_)
    {
        impl_->demux_task_->setSpeed(speed);
    }
    if (impl_->audio_decode_task_)
    {
        impl_->audio_decode_task_->setSpeed(speed);
    }
    if (impl_->audio_play_task_)
    {
        const double media_sec = impl_->demux_task_ ? impl_->demux_task_->getCurrentTime() : -1.0;
        impl_->audio_play_task_->setSpeed(speed, media_sec);
    }

    LOGI("设置播放速度: " << speed << "x");
}

double LocalPlayer::getSpeed() const
{
    return impl_->speed_.load();
}

double LocalPlayer::getDuration() const
{
    return impl_->duration_;
}

double LocalPlayer::getCurrentTime() const
{
    if (impl_->demux_task_)
    {
        return impl_->demux_task_->getCurrentTime();
    }
    return 0.0;
}

bool LocalPlayer::isPlaying() const
{
    return impl_->is_playing_;
}

bool LocalPlayer::isPaused() const
{
    return impl_->is_paused_;
}

bool LocalPlayer::isFinished() const
{
    return impl_->is_finished_;
}

int LocalPlayer::getWidth() const
{
    return impl_->video_width_;
}

int LocalPlayer::getHeight() const
{
    return impl_->video_height_;
}

bool LocalPlayer::hasAudio() const
{
    return impl_->audio_play_task_ != nullptr;
}

void LocalPlayer::setVolume(double volume)
{
    if (volume < 0.0)
    {
        volume = 0.0;
    }
    else if (volume > 1.0)
    {
        volume = 1.0;
    }

    if (impl_->audio_play_task_)
    {
        impl_->audio_play_task_->setVolume(volume);
    }
}

double LocalPlayer::getVolume() const
{
    if (impl_->audio_play_task_ && impl_->audio_play_task_->getPlayer())
    {
        return impl_->audio_play_task_->getPlayer()->getVolume();
    }
    return 1.0;
}

void LocalPlayer::PImpl::controlLoop()
{
    LOGI("控制线程启动");

    while (!should_stop_ && is_playing_)
    {
        if (is_paused_)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        if (demux_task_ && demux_task_->isEofReached())
        {
            LOGI("文件读取完成，等待队列清空...");

            for (int i = 0; i < 10; i++)
            {
                const bool video_drained =
                    (!decode_task_ || decode_task_->getQueueSize() == 0) &&
                    (!display_task_ || display_task_->getQueueSize() == 0);
                const bool audio_drained =
                    (!audio_decode_task_ || audio_decode_task_->getQueueSize() == 0) &&
                    (!audio_play_task_ || audio_play_task_->getQueueSize() == 0);

                if (video_drained && audio_drained)
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

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    LOGI("控制线程结束");
}
