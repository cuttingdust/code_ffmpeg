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

    // 先停止所有线程标志
    duration_monitor_running_ = false;
    segment_monitor_running_  = false;
    is_recording_             = false;

    // 等待线程结束
    if (duration_thread_.joinable())
    {
        duration_thread_.join();
    }
    if (segment_thread_.joinable())
    {
        segment_thread_.join();
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
    demux_task_  = XDemuxTask::create();
    decode_task_ = XVideoDecodeTask::create();
    encode_task_ = XEncodeTask::create();
    muxer_task_  = XMuxerTask::create();

    demux_task_->setIdleTimeoutMs(5000);
    decode_task_->setIdleTimeoutMs(3000);
    encode_task_->setIdleTimeoutMs(3000);
    muxer_task_->setIdleTimeoutMs(3000);

    demux_task_->setNext(decode_task_);
    decode_task_->setNext(encode_task_);
    encode_task_->setNext(muxer_task_);

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

bool RecordClient::openUrlAndGetStream()
{
    if (!demux_task_->open(url_))
    {
        LOGE("打开URL失败: " << url_);
        return false;
    }

    demux_task_->setRtspOptions(true, 5000);

    video_stream_ = demux_task_->getVideoStream();
    if (!video_stream_)
    {
        LOGE("未找到视频流");
        return false;
    }

    /// 获取原始视频流的帧率并保存到 src_framerate_
    src_framerate_ = video_stream_->avg_frame_rate;
    if (src_framerate_.num == 0 || src_framerate_.den == 0)
    {
        src_framerate_ = video_stream_->r_frame_rate;
    }
    if (src_framerate_.num == 0 || src_framerate_.den == 0)
    {
        src_framerate_ = { encode_config_.framerate, 1 };
    }

    LOGI("原始流帧率: " << src_framerate_.num << "/" << src_framerate_.den);
    return true;
}

bool RecordClient::initEncoderAndMuxer(const std::string& output_file)
{
    if (!initDecoder())
    {
        LOGE("初始化解码器失败");
        return false;
    }

    // 使用临时配置，不修改原配置
    EncoderConfig temp_config = encode_config_;
    if (src_framerate_.num > 0 && src_framerate_.den > 0)
    {
        temp_config.framerate = src_framerate_.num / src_framerate_.den;
    }

    if (!encode_task_->init(temp_config))
    {
        LOGE("初始化编码器失败");
        return false;
    }

    AVCodecContext* enc_ctx   = encode_task_->getCodecContext();
    AVRational      time_base = enc_ctx->time_base;

    LOGI("编码器时间基: " << time_base.num << "/" << time_base.den);
    LOGI("使用帧率: " << src_framerate_.num << "/" << src_framerate_.den);

    if (!muxer_task_->init(output_file, enc_ctx, time_base, src_framerate_))
    {
        LOGE("初始化封装器失败");
        return false;
    }

    return true;
}

bool RecordClient::resetAndReopen(const std::string& new_output_file)
{
    // 停止并等待所有任务
    if (demux_task_)
        demux_task_->stop();
    if (decode_task_)
        decode_task_->stop();
    if (encode_task_)
        encode_task_->stop();
    if (muxer_task_)
        muxer_task_->stop();

    if (demux_task_)
        demux_task_->wait();
    if (decode_task_)
        decode_task_->wait();
    if (encode_task_)
        encode_task_->wait();
    if (muxer_task_)
        muxer_task_->wait();

    // 重置任务
    if (demux_task_)
        demux_task_->reset();
    if (decode_task_)
        decode_task_->reset();
    if (encode_task_)
        encode_task_->reset();
    if (muxer_task_)
        muxer_task_->reset();

    // 重新打开 URL
    if (!demux_task_->open(url_))
    {
        LOGE("重新打开URL失败: " << url_);
        return false;
    }

    demux_task_->setRtspOptions(true, 5000);

    video_stream_ = demux_task_->getVideoStream();
    if (!video_stream_)
    {
        LOGE("重新获取视频流失败");
        return false;
    }

    // 重新获取帧率
    src_framerate_ = video_stream_->avg_frame_rate;
    if (src_framerate_.num == 0 || src_framerate_.den == 0)
    {
        src_framerate_ = video_stream_->r_frame_rate;
    }
    if (src_framerate_.num == 0 || src_framerate_.den == 0)
    {
        src_framerate_ = { encode_config_.framerate, 1 };
    }

    if (!initDecoder())
    {
        LOGE("重新初始化解码器失败");
        return false;
    }

    // 使用保存的原始帧率
    EncoderConfig temp_config = encode_config_;
    if (src_framerate_.num > 0 && src_framerate_.den > 0)
    {
        temp_config.framerate = src_framerate_.num / src_framerate_.den;
    }

    if (!encode_task_->init(temp_config))
    {
        LOGE("重新初始化编码器失败");
        return false;
    }

    AVCodecContext* enc_ctx   = encode_task_->getCodecContext();
    AVRational      time_base = enc_ctx->time_base;

    if (!muxer_task_->init(new_output_file, enc_ctx, time_base, src_framerate_))
    {
        LOGE("重新初始化封装器失败");
        return false;
    }

    // 重新启动线程
    demux_task_->start();
    decode_task_->start();
    encode_task_->start();
    muxer_task_->start();

    return true;
}

bool RecordClient::start()
{
    LOGI("录制客户端启动...");
    setState(MediaClientState::CONNECTING);

    if (!openUrlAndGetStream())
    {
        setState(MediaClientState::ERROR);
        return false;
    }

    if (!initEncoderAndMuxer(output_file_))
    {
        setState(MediaClientState::ERROR);
        return false;
    }

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
    segment_monitor_running_  = false;

    if (encode_task_)
    {
        LOGI("刷新编码器...");
        encode_task_->flush();
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    if (muxer_task_)
    {
        LOGI("关闭封装器，写入 moov...");
        muxer_task_->close();
        muxer_task_.reset();
    }

    if (encode_task_)
    {
        encode_task_->close();
        encode_task_.reset();
    }

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

    if (duration_sec_ > 0)
    {
        int wait_count = 0;
        while (muxer_task_ && !muxer_task_->hasKeyFrameWritten() && wait_count < 100)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            wait_count++;
        }

        if (muxer_task_ && muxer_task_->hasKeyFrameWritten())
        {
            LOGI("关键帧已写入，开始计时 " << duration_sec_ << " 秒");
            duration_monitor_running_ = true;
            if (duration_thread_.joinable())
            {
                duration_thread_.join();
            }
            duration_thread_ = std::thread(&RecordClient::durationMonitorThread, this);
            duration_thread_.detach();
        }
        else
        {
            LOGW("等待关键帧超时，录制可能无法正常播放");
        }
    }

    return true;
}

bool RecordClient::startSegmentRecording(const std::string& prefix, int segment_sec, int total_sec)
{
    if (segment_sec <= 0)
    {
        LOGE("分段时长必须大于0");
        return false;
    }

    segment_prefix_    = prefix;
    segment_duration_  = segment_sec;
    total_segment_sec_ = total_sec;
    segment_index_     = 1;

    std::string first_file = generateSegmentFilename();
    output_file_           = first_file;
    duration_sec_          = segment_sec;
    start_time_            = std::chrono::steady_clock::now();

    if (!start())
    {
        return false;
    }

    is_recording_ = true;

    int wait_count = 0;
    while (muxer_task_ && !muxer_task_->hasKeyFrameWritten() && wait_count < 100)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        wait_count++;
    }

    segment_monitor_running_ = true;
    if (segment_thread_.joinable())
    {
        segment_thread_.join();
    }
    segment_thread_ = std::thread(&RecordClient::segmentMonitorThread, this);

    LOGI("开始分段录制: 前缀=" << prefix << ", 每段=" << segment_sec << "秒");
    return true;
}

void RecordClient::stopRecording()
{
    if (!is_recording_)
    {
        return;
    }

    LOGI("停止录制...");

    // 先停止监控线程标志
    segment_monitor_running_  = false;
    duration_monitor_running_ = false;
    is_recording_             = false;

    // 等待关键帧
    int wait_count = 0;
    while (muxer_task_ && !muxer_task_->hasKeyFrameWritten() && wait_count < 30)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        wait_count++;
    }

    stop();
}

int RecordClient::getPacketCount() const
{
    if (!muxer_task_)
    {
        return 0;
    }
    return muxer_task_->getPacketCount();
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

    // 分段睡眠，每秒检查一次状态
    for (int i = 0; i < duration_sec_ && duration_monitor_running_ && is_recording_; i++)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    if (duration_monitor_running_ && is_recording_)
    {
        LOGI("录制时长达到 " << duration_sec_ << " 秒，自动停止");
        stopRecording();
    }

    LOGI("时长监控线程结束");
}

void RecordClient::segmentMonitorThread()
{
    LOGI("分段监控线程启动，每段 " << segment_duration_ << " 秒");

    while (segment_monitor_running_ && is_recording_)
    {
        // 分段睡眠，每秒检查一次状态
        for (int i = 0; i < segment_duration_ && segment_monitor_running_ && is_recording_; i++)
        {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        if (!segment_monitor_running_ || !is_recording_)
        {
            LOGI("分段监控线程: 录制已停止，退出");
            break;
        }

        LOGI("达到分段时长，切换文件...");
        switchSegment();
    }

    LOGI("分段监控线程结束");
}

void RecordClient::switchSegment()
{
    // 多重检查
    if (!is_recording_)
    {
        LOGI("switchSegment: 录制已停止");
        return;
    }

    if (!muxer_task_)
    {
        LOGW("switchSegment: muxer_task_ 为空");
        return;
    }

    int current_packets = getPacketCount();
    LOGI("当前段写入包数: " << current_packets);

    // 关闭当前封装器
    if (muxer_task_)
    {
        muxer_task_->close();
        muxer_task_.reset();
    }

    if (encode_task_)
    {
        encode_task_->close();
        encode_task_.reset();
    }

    // 检查是否达到总录制时长
    if (total_segment_sec_ > 0 && segment_index_ * segment_duration_ >= total_segment_sec_)
    {
        LOGI("达到总录制时长，停止分段录制");
        segment_monitor_running_ = false;
        is_recording_            = false;
        return;
    }

    // 再次检查录制状态（可能在等待期间被停止）
    if (!is_recording_)
    {
        LOGI("switchSegment: 录制已停止，不再继续下一段");
        return;
    }

    segment_index_++;
    std::string next_file = generateSegmentFilename();
    LOGI("开始下一段: " << next_file);

    encode_task_ = XEncodeTask::create();
    muxer_task_  = XMuxerTask::create();

    if (decode_task_)
    {
        decode_task_->setNext(encode_task_);
    }
    encode_task_->setNext(muxer_task_);

    auto error_cb = [this](const std::string& msg) { handleError(msg); };
    encode_task_->setErrorCallback(error_cb);
    muxer_task_->setErrorCallback(error_cb);

    if (!resetAndReopen(next_file))
    {
        LOGE("重新打开下一段失败");
        segment_monitor_running_ = false;
        is_recording_            = false;
    }
    else
    {
        LOGI("下一段录制已启动");
    }
}

auto RecordClient::generateSegmentFilename() -> std::string
{
    auto    now    = std::chrono::system_clock::now();
    auto    time_t = std::chrono::system_clock::to_time_t(now);
    std::tm bt;
#ifdef _WIN32
    localtime_s(&bt, &time_t);
#else
    localtime_r(&time_t, &bt);
#endif

    char buf[256];
    strftime(buf, sizeof(buf), "_%Y%m%d_%H%M%S", &bt);

    return segment_prefix_ + std::to_string(segment_index_) + buf + ".mp4";
}

void RecordClient::reconnectImpl()
{
    LOGI("RecordClient 重连实现");

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

    if (!resetAndReopen(output_file_))
    {
        LOGE("重连失败");
        setState(MediaClientState::ERROR);
        return;
    }

    setState(MediaClientState::CONNECTED);
    LOGI("RecordClient 重连成功");
}
