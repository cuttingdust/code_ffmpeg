#include "RecordClient.h"
#include "AVLog.h"

RecordClient::RecordClient()
{
    LOGI("录制客户端创建");
    use_hardware_ = false;
    initTasks();
}

RecordClient::~RecordClient()
{
    LOGI("录制客户端销毁");
    duration_monitor_running_ = false;
    is_recording_             = false;
    if (duration_thread_.joinable())
    {
        duration_thread_.join();
    }
    stop();
    wait();
}

auto RecordClient::create() -> std::shared_ptr<RecordClient>
{
    return std::make_shared<RecordClient>();
}

void RecordClient::initTasks()
{
    // 创建任务
    demux_task_  = XDemuxTask::create();
    decode_task_ = XDecodeTask::create();
    encode_task_ = XEncodeTask::create();
    muxer_task_  = XMuxerTask::create();

    // 设置超时
    demux_task_->setIdleTimeoutMs(5000);
    decode_task_->setIdleTimeoutMs(3000);
    encode_task_->setIdleTimeoutMs(3000);
    muxer_task_->setIdleTimeoutMs(3000);

    // 设置任务链
    demux_task_->setNext(decode_task_);
    decode_task_->setNext(encode_task_);
    encode_task_->setNext(muxer_task_);

    // 设置错误回调
    auto error_cb = [this](const std::string& msg) { handleError(msg); };

    demux_task_->setErrorCallback(error_cb);
    decode_task_->setErrorCallback(error_cb);
    encode_task_->setErrorCallback(error_cb);
    muxer_task_->setErrorCallback(error_cb);
}

void RecordClient::startTasks()
{
    XMediaClient::startTasks();
    if (encode_task_)
        encode_task_->start();
    if (muxer_task_)
        muxer_task_->start();
}

void RecordClient::stopTasks()
{
    XMediaClient::stopTasks();
    if (encode_task_)
        encode_task_->stop();
    if (muxer_task_)
        muxer_task_->stop();
}

void RecordClient::resetTasks()
{
    XMediaClient::resetTasks();
    if (encode_task_)
        encode_task_->reset();
    if (muxer_task_)
        muxer_task_->reset();
}

void RecordClient::reconnectImpl()
{
    LOGI("RecordClient 重连实现");

    // 停止当前任务
    stopTasks();

    if (demux_task_)
        demux_task_->wait();
    if (decode_task_)
        decode_task_->wait();
    if (encode_task_)
        encode_task_->wait();
    if (muxer_task_)
        muxer_task_->wait();

    resetTasks();

    // 重新打开 URL
    if (!demux_task_->open(url_))
    {
        LOGE("重连打开URL失败: " << url_);
        setState(MediaClientState::ERROR);
        return;
    }

    demux_task_->setRtspOptions(true, 5000);

    video_stream_ = demux_task_->getVideoStream();
    if (!video_stream_)
    {
        LOGE("重连未找到视频流");
        setState(MediaClientState::ERROR);
        return;
    }

    // 获取原始视频流的帧率
    AVRational src_frame_rate = video_stream_->avg_frame_rate;
    if (src_frame_rate.num == 0 || src_frame_rate.den == 0)
    {
        src_frame_rate = video_stream_->r_frame_rate;
    }
    if (src_frame_rate.num == 0 || src_frame_rate.den == 0)
    {
        src_frame_rate = { .num = encode_config_.framerate, .den = 1 };
    }
    LOGI("原始流帧率: " << src_frame_rate.num << "/" << src_frame_rate.den);

    // 初始化解码器
    if (!initDecoder())
    {
        LOGE("重连初始化解码器失败");
        setState(MediaClientState::ERROR);
        return;
    }

    // 初始化编码器，使用原始流帧率
    encode_config_.framerate = src_frame_rate.num / src_frame_rate.den;
    if (!encode_task_->init(encode_config_))
    {
        LOGE("重连初始化编码器失败");
        setState(MediaClientState::ERROR);
        return;
    }

    // 获取编码器的时间基
    AVCodecContext* enc_ctx    = encode_task_->getCodecContext();
    AVRational      time_base  = enc_ctx->time_base;
    AVRational      frame_rate = src_frame_rate;

    LOGI("编码器时间基: " << time_base.num << "/" << time_base.den);
    LOGI("使用帧率: " << frame_rate.num << "/" << frame_rate.den);

    if (!muxer_task_->init(output_file_, enc_ctx, time_base, frame_rate))
    {
        LOGE("重连初始化封装器失败");
        setState(MediaClientState::ERROR);
        return;
    }

    startTasks();
    setState(MediaClientState::CONNECTED);
    LOGI("RecordClient 重连成功");
}

bool RecordClient::start()
{
    LOGI("录制客户端启动...");
    setState(MediaClientState::CONNECTING);

    // 打开解封装
    if (!demux_task_->open(url_))
    {
        LOGE("打开URL失败: " << url_);
        setState(MediaClientState::ERROR);
        return false;
    }

    demux_task_->setRtspOptions(true, 5000);

    // 获取视频流
    video_stream_ = demux_task_->getVideoStream();
    if (!video_stream_)
    {
        LOGE("未找到视频流");
        setState(MediaClientState::ERROR);
        return false;
    }

    // 获取原始视频流的帧率（最准确）
    AVRational src_frame_rate = video_stream_->avg_frame_rate;
    if (src_frame_rate.num == 0 || src_frame_rate.den == 0)
    {
        src_frame_rate = video_stream_->r_frame_rate;
    }
    if (src_frame_rate.num == 0 || src_frame_rate.den == 0)
    {
        src_frame_rate = { .num = encode_config_.framerate, .den = 1 };
    }

    LOGI("原始流帧率: " << src_frame_rate.num << "/" << src_frame_rate.den);

    // 初始化解码器
    if (!initDecoder())
    {
        LOGE("初始化解码器失败");
        setState(MediaClientState::ERROR);
        return false;
    }

    // 初始化编码器，使用原始流帧率
    encode_config_.framerate = src_frame_rate.num / src_frame_rate.den;
    if (!encode_task_->init(encode_config_))
    {
        LOGE("初始化编码器失败");
        setState(MediaClientState::ERROR);
        return false;
    }

    // 获取编码器的时间基
    AVCodecContext* enc_ctx    = encode_task_->getCodecContext();
    AVRational      time_base  = enc_ctx->time_base;
    AVRational      frame_rate = src_frame_rate;

    LOGI("编码器时间基: " << time_base.num << "/" << time_base.den);
    LOGI("使用帧率: " << frame_rate.num << "/" << frame_rate.den);

    // 初始化封装器
    if (!muxer_task_->init(output_file_, enc_ctx, time_base, frame_rate))
    {
        LOGE("初始化封装器失败");
        setState(MediaClientState::ERROR);
        return false;
    }

    // 启动所有任务
    startTasks();

    setState(MediaClientState::CONNECTED);
    LOGI("录制客户端启动成功");
    return true;
}

void RecordClient::stop()
{
    LOGI("录制客户端停止");
    is_recording_             = false;
    duration_monitor_running_ = false;
    setState(MediaClientState::DISCONNECTED);
    stopTasks();
}

void RecordClient::wait()
{
    if (demux_task_)
        demux_task_->wait();
    if (decode_task_)
        decode_task_->wait();
    if (encode_task_)
        encode_task_->wait();
    if (muxer_task_)
        muxer_task_->wait();
    LOGI("录制客户端已停止");
}

bool RecordClient::startRecording(const std::string& output_file, int duration_sec)
{
    output_file_  = output_file;
    duration_sec_ = duration_sec;
    start_time_   = std::chrono::steady_clock::now();

    if (!start())
    {
        return false;
    }

    is_recording_ = true;

    // 如果指定了时长，启动监控线程
    if (duration_sec_ > 0)
    {
        duration_monitor_running_ = true;
        duration_thread_          = std::thread(&RecordClient::durationMonitorThread, this);
        duration_thread_.detach();
    }

    return true;
}

void RecordClient::stopRecording()
{
    is_recording_             = false;
    duration_monitor_running_ = false;
    stop();
}

int RecordClient::getPacketCount() const
{
    return muxer_task_ ? muxer_task_->getPacketCount() : 0;
}

void RecordClient::setEncodeConfig(const EncoderConfig& config)
{
    encode_config_ = config;
}

bool RecordClient::isRecording() const
{
    return is_recording_;
}

void RecordClient::durationMonitorThread()
{
    LOGI("时长监控线程启动，将在 " << duration_sec_ << " 秒后停止录制");
    std::this_thread::sleep_for(std::chrono::seconds(duration_sec_));

    if (duration_monitor_running_ && is_recording_)
    {
        LOGI("录制时长达到 " << duration_sec_ << " 秒，自动停止");
        stopRecording();
    }

    LOGI("时长监控线程结束");
}
