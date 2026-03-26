#include "XRecordTask.h"
#include "AVLog.h"

XRecordTask::XRecordTask()
{
    setName("RecordTask");
    setMaxQueueSize(500);
    LOGD("录制任务创建");
}

XRecordTask::~XRecordTask()
{
    LOGD("录制任务销毁");
    endRecord();
}

bool XRecordTask::beginRecord(const std::string& filename, AVStream* video_stream, int duration_sec)
{
    if (muxer_)
    {
        LOGW("已经在录制中");
        return false;
    }

    if (!video_stream)
    {
        LOGE("视频流为空");
        return false;
    }

    filename_     = filename;
    video_stream_ = video_stream;
    time_base_    = video_stream->time_base;
    start_pts_    = AV_NOPTS_VALUE;
    end_pts_      = AV_NOPTS_VALUE;
    packet_count_ = 0;
    duration_sec_ = duration_sec;
    need_stop_    = false;
    start_time_   = std::chrono::steady_clock::now();

    try
    {
        muxer_ = std::make_unique<Muxer>(filename);
        if (!muxer_->open())
        {
            LOGE("无法创建输出文件: " << filename);
            muxer_.reset();
            return false;
        }

        int out_idx = muxer_->addVideoStream(video_stream);
        if (out_idx < 0)
        {
            LOGE("添加视频流失败");
            muxer_.reset();
            return false;
        }

        if (muxer_->writeHeader() < 0)
        {
            LOGE("写入文件头失败");
            muxer_.reset();
            return false;
        }

        LOGI("开始录制: " << filename
                          << ", 时长: " << (duration_sec > 0 ? std::to_string(duration_sec) + "秒" : "无限"));
        return true;
    }
    catch (const std::exception& e)
    {
        LOGE("创建封装器异常: " << e.what());
        muxer_.reset();
        return false;
    }
}

void XRecordTask::endRecord()
{
    if (!muxer_)
    {
        return;
    }

    LOGI("结束录制: " << filename_ << ", 共写入 " << packet_count_ << " 个包");
    muxer_->writeTrailer();
    muxer_.reset();
    video_stream_ = nullptr;
    start_pts_    = AV_NOPTS_VALUE;
    end_pts_      = AV_NOPTS_VALUE;
    packet_count_ = 0;
    need_stop_    = false;
    filename_.clear();
}

XRecordTask::Status XRecordTask::getStatus() const
{
    Status status;
    status.is_recording = isRecording();
    status.packet_count = packet_count_;
    status.total_sec    = duration_sec_;
    status.filename     = filename_;

    if (isRecording() && start_pts_ != AV_NOPTS_VALUE)
    {
        auto now            = std::chrono::steady_clock::now();
        auto elapsed        = std::chrono::duration_cast<std::chrono::seconds>(now - start_time_).count();
        status.recorded_sec = static_cast<int>(elapsed);
        if (duration_sec_ > 0 && status.recorded_sec > duration_sec_)
        {
            status.recorded_sec = duration_sec_;
        }
    }

    return status;
}

void XRecordTask::feedPacket(PacketWrapper::Ptr pkt)
{
    if (!muxer_ || !pkt || !pkt->get())
    {
        return;
    }

    AVPacket* av_pkt = pkt->get();

    /// 等待关键帧才开始写入
    if (start_pts_ == AV_NOPTS_VALUE)
    {
        if (av_pkt->flags & AV_PKT_FLAG_KEY)
        {
            start_pts_  = av_pkt->pts;
            start_time_ = std::chrono::steady_clock::now();

            /// 计算结束PTS
            if (duration_sec_ > 0 && time_base_.num > 0 && time_base_.den > 0)
            {
                double pts_per_second = static_cast<double>(time_base_.den) / time_base_.num;
                end_pts_              = start_pts_ + static_cast<int64_t>(duration_sec_ * pts_per_second);
                LOGI("找到关键帧，开始录制，开始PTS: " << start_pts_ << ", 结束PTS: " << end_pts_);
            }
            else
            {
                LOGI("找到关键帧，开始录制，PTS: " << start_pts_);
            }
        }
        else
        {
            return; /// 丢弃非关键帧
        }
    }

    /// 检查是否达到录制时长
    bool time_reached = false;

    if (duration_sec_ > 0)
    {
        /// 方式1：使用PTS判断
        if (av_pkt->pts > end_pts_)
        {
            time_reached = true;
        }
        /// 方式2：使用系统时间判断（备用）
        else if (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start_time_)
                         .count() >= duration_sec_)
        {
            time_reached = true;
        }
    }

    if (time_reached)
    {
        /// 等待关键帧再停止，确保GOP完整
        if (av_pkt->flags & AV_PKT_FLAG_KEY)
        {
            LOGI("达到录制时长，在关键帧处停止");
            endRecord();
        }
        else
        {
            need_stop_ = true;
        }
        return;
    }

    /// 如果需要停止且是关键帧
    if (need_stop_ && (av_pkt->flags & AV_PKT_FLAG_KEY))
    {
        LOGI("遇到关键帧，停止录制");
        endRecord();
        return;
    }

    /// 写入文件
    int ret = muxer_->writePacket(av_pkt, video_stream_, start_pts_);
    if (ret < 0)
    {
        char err_buf[256];
        av_strerror(ret, err_buf, sizeof(err_buf));
        LOGE("写入包失败: " << err_buf);

        /// 磁盘满等致命错误，停止录制
        if (ret == AVERROR(ENOSPC) || ret == AVERROR(EIO))
        {
            LOGE("写入错误，停止录制");
            endRecord();
        }
    }
    else
    {
        packet_count_++;
    }
}

void XRecordTask::reset()
{
    XTask::reset();
    endRecord();
}

void XRecordTask::process()
{
    LOGI("录制任务线程启动");

    while (!shouldStop())
    {
        // 从队列取出数据包
        auto pkt = popPacket();
        if (pkt)
        {
            // 调用 feedPacket 处理
            feedPacket(std::move(pkt));
        }
        else
        {
            // 没有数据时短暂休眠，避免CPU空转
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    LOGI("录制任务线程结束");
}

IMPLEMENT_CREATE(XRecordTask)
