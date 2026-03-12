#include "VideoCutter.h"
#include "AVConst.h"
#include <iostream>

VideoCutter::VideoCutter() = default;

VideoCutter::~VideoCutter() = default;

void VideoCutter::setParams(const CutParams& params)
{
    params_ = params;
}

void VideoCutter::setProgressCallback(CutProgressCallback callback)
{
    progress_callback_ = std::move(callback);
}

bool VideoCutter::calculatePTS()
{
    if (!video_stream_)
    {
        return false;
    }

    /// 计算视频PTS
    if (video_stream_->time_base.num > 0)
    {
        double t =
                static_cast<double>(video_stream_->time_base.den) / static_cast<double>(video_stream_->time_base.num);
        video_start_pts_ = params_.start_time * t;
        video_end_pts_   = params_.end_time * t;

        std::cout << "\n视频时间基: " << video_stream_->time_base.num << "/" << video_stream_->time_base.den
                  << ", 1秒 = " << t << " 单位" << std::endl;
        std::cout << "开始 PTS: " << video_start_pts_ << ", 结束 PTS: " << video_end_pts_ << std::endl;
    }

    /// 计算音频PTS
    if (audio_stream_ && audio_stream_->time_base.num > 0)
    {
        double t =
                static_cast<double>(audio_stream_->time_base.den) / static_cast<double>(audio_stream_->time_base.num);
        audio_start_pts_ = params_.start_time * t;
        std::cout << "音频开始 PTS: " << audio_start_pts_ << std::endl;
    }

    stats_.start_pts = video_start_pts_;
    stats_.end_pts   = video_end_pts_;
    stats_.duration  = params_.end_time - params_.start_time;

    return true;
}

bool VideoCutter::seekToStart()
{
    if (!demuxer_ || !video_stream_)
    {
        return false;
    }

    bool seek_ret = demuxer_->seek(params_.start_time, video_stream_->index, AVSEEK_FLAG_FRAME | AVSEEK_FLAG_BACKWARD);
    if (!seek_ret)
    {
        std::cerr << "定位失败" << std::endl;
        return false;
    }

    return true;
}

bool VideoCutter::processVideoPacket(AVPacket* pkt, AVStream* in_stream, int out_stream_index, int64_t offset_pts)
{
    stats_.video_packets++;

    /// 显示进度
    if (progress_callback_)
    {
        progress_callback_(stats_.video_packets, stats_.audio_packets, pkt->pts);
    }

    /// 检查是否超出结束时间
    if (pkt->pts > video_end_pts_)
    {
        std::cout << "\n达到结束时间" << std::endl;
        return false; /// 停止截取
    }

    /// 写入数据包
    int ret = muxer_->writePacket(pkt, pkt->stream_index, out_stream_index, in_stream->time_base, offset_pts);
    if (ret < 0)
    {
        PrintErr(ret);
    }

    return true; /// 继续截取
}

bool VideoCutter::processAudioPacket(AVPacket* pkt, AVStream* in_stream, int out_stream_index, int64_t offset_pts)
{
    stats_.audio_packets++;

    /// 写入数据包
    int ret = muxer_->writePacket(pkt, pkt->stream_index, out_stream_index, in_stream->time_base, offset_pts);
    if (ret < 0)
    {
        PrintErr(ret);
    }

    return true; /// 继续截取
}

bool VideoCutter::cut()
{
    try
    {
        /// 1. 打开输入文件
        demuxer_ = Demuxer::create(params_.input_file);
        if (!demuxer_->open())
        {
            std::cerr << "无法打开输入文件: " << params_.input_file << std::endl;
            return false;
        }

        /// 打印输入文件信息
        demuxer_->dumpInfo();

        /// 2. 获取输入流
        video_stream_ = demuxer_->getVideoStream();
        audio_stream_ = demuxer_->getAudioStream();

        if (!video_stream_)
        {
            std::cerr << "未找到视频流" << std::endl;
            return false;
        }

        /// 3. 创建输出文件
        muxer_ = Muxer::create(params_.output_file);
        if (!muxer_->open())
        {
            std::cerr << "无法创建输出文件: " << params_.output_file << std::endl;
            return false;
        }

        /// 4. 添加输出流
        if (params_.copy_video)
        {
            out_video_idx_ = muxer_->addVideoStream(video_stream_);
        }

        if (params_.copy_audio && audio_stream_)
        {
            out_audio_idx_ = muxer_->addAudioStream(audio_stream_);
        }

        /// 5. 写入文件头
        int ret = muxer_->writeHeader();
        CHECK_ERR(ret)

        /// 打印输出文件信息
        muxer_->dumpInfo();

        /// 6. 计算PTS
        if (!calculatePTS())
        {
            std::cerr << "计算PTS失败" << std::endl;
            return false;
        }

        /// 7. 定位到开始时间
        if (!seekToStart())
        {
            return false;
        }

        /// 8. 读取并处理数据包
        PacketWrapper pkt;
        bool          continue_cut = true;

        std::cout << "\n开始截取 " << params_.start_time << " ~ " << params_.end_time << " 秒..." << std::endl;

        while (continue_cut)
        {
            /// 读取数据包
            ret = demuxer_->readPacket(pkt);

            if (ret == AVERROR_EOF)
            {
                std::cout << "\n文件读取完成" << std::endl;
                break;
            }
            else if (ret < 0)
            {
                PrintErr(ret);
                break;
            }

            /// 获取输入流
            AVStream* in_stream = demuxer_->getStream(pkt->stream_index);
            if (!in_stream)
            {
                pkt.unref();
                continue;
            }

            /// 处理视频包
            if (video_stream_ && pkt->stream_index == video_stream_->index && params_.copy_video)
            {
                continue_cut = processVideoPacket(pkt, in_stream, out_video_idx_, video_start_pts_);
            }
            /// 处理音频包
            else if (audio_stream_ && pkt->stream_index == audio_stream_->index && params_.copy_audio)
            {
                continue_cut = processAudioPacket(pkt, in_stream, out_audio_idx_, audio_start_pts_);
            }

            pkt.unref();
        }

        /// 9. 写入文件尾
        ret = muxer_->writeTrailer();
        CHECK_ERR(ret);

        /// 打印统计信息
        std::cout << "\n\n========== 截取完成 ==========" << std::endl;
        std::cout << "输出文件: " << params_.output_file << std::endl;
        std::cout << "视频包数: " << stats_.video_packets << std::endl;
        std::cout << "音频包数: " << stats_.audio_packets << std::endl;
        std::cout << "截取时长: " << stats_.duration << " 秒" << std::endl;

        return true;
    }
    catch (const std::exception& e)
    {
        std::cerr << "截取错误: " << e.what() << std::endl;
        return false;
    }
}
