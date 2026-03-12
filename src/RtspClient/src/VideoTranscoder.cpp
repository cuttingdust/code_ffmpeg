#include "VideoTranscoder.h"
#include "AVConst.h"
#include <iostream>

VideoTranscoder::VideoTranscoder() = default;

VideoTranscoder::~VideoTranscoder() = default;

void VideoTranscoder::setParams(const TranscodeParams& params)
{
    params_ = params;
}

void VideoTranscoder::setProgressCallback(TranscodeProgressCallback callback)
{
    progress_callback_ = std::move(callback);
}

bool VideoTranscoder::initDecoder()
{
    /// 配置解码器
    DecoderConfig config;
    config.codec_id     = AV_CODEC_ID_H264;
    config.thread_count = 16;

    /// 硬件加速配置
    config.hardware.enable               = params_.enable_hardware_decode;
    config.hardware.auto_select          = true;
    config.hardware.preferred_type       = params_.hw_type;
    config.hardware.transfer_to_software = true; /// 转到CPU进行编码

    decoder_ = VideoDecoder::create(config);

    /// 设置解码参数
    if (!decoder_->set_parameters_from_stream(video_stream_))
    {
        std::cerr << "设置解码器参数失败" << std::endl;
        return false;
    }

    decoder_->open();
    return true;
}

bool VideoTranscoder::initEncoder()
{
    /// 获取输入视频信息
    input_fps_ = av_q2d(video_stream_->avg_frame_rate);
    if (input_fps_ <= 0)
    {
        input_fps_ = av_q2d(video_stream_->r_frame_rate);
    }

    auto& enc_cfg = params_.video_config;

    /// 设置编码参数
    if (enc_cfg.codec_id == AV_CODEC_ID_NONE)
    {
        enc_cfg.codec_id = AV_CODEC_ID_H265;
    }

    if (enc_cfg.width == 0 || enc_cfg.height == 0)
    {
        enc_cfg.width  = video_stream_->codecpar->width;
        enc_cfg.height = video_stream_->codecpar->height;
    }

    if (enc_cfg.framerate == 0)
    {
        enc_cfg.framerate = static_cast<int>(input_fps_);
    }
    output_fps_ = enc_cfg.framerate;

    /// 创建编码器
    encoder_ = VideoEncoder::create(enc_cfg.codec_id, enc_cfg);

    /// 获取编码器上下文
    AVCodecContext* enc_ctx = encoder_->get_ctx();
    encoder_time_base_      = enc_ctx->time_base;

    /// 添加视频流到输出文件
    out_video_idx_ = muxer_->addStream(enc_ctx, encoder_time_base_);

    /// 获取输出流
    AVStream* out_stream = muxer_->getStream(out_video_idx_);

    /// 使用输入流的时间基作为输出流的时间基
    /// 输入流的时间基是 1/12800，这是 MP4 的标准时间基
    out_stream->time_base = video_stream_->time_base;
    mux_time_base_        = out_stream->time_base;

    std::cout << "编码器时间基: " << encoder_time_base_.num << "/" << encoder_time_base_.den << std::endl;
    std::cout << "封装器时间基: " << mux_time_base_.num << "/" << mux_time_base_.den << " (与输入一致)" << std::endl;

    /// 添加音频流（直通）
    if (params_.transcode_audio && audio_stream_)
    {
        out_audio_idx_ = muxer_->addAudioStream(audio_stream_);
    }

    std::cout << "\n编码器配置:" << std::endl;
    enc_cfg.print();

    return true;
}

bool VideoTranscoder::calculatePTS()
{
    if (!video_stream_)
        return false;

    if (video_stream_->time_base.num > 0)
    {
        double t =
                static_cast<double>(video_stream_->time_base.den) / static_cast<double>(video_stream_->time_base.num);
        video_start_pts_ = params_.start_time * t;
        video_end_pts_   = params_.end_time * t;

        std::cout << "\n输入时间基: " << video_stream_->time_base.num << "/" << video_stream_->time_base.den
                  << ", 1秒 = " << t << " 单位" << std::endl;
        std::cout << "开始 PTS: " << video_start_pts_ << ", 结束 PTS: " << video_end_pts_ << std::endl;
    }

    stats_.start_pts = video_start_pts_;
    stats_.end_pts   = video_end_pts_;
    stats_.duration  = params_.end_time - params_.start_time;

    return true;
}

bool VideoTranscoder::seekToStart()
{
    if (!demuxer_ || !video_stream_)
    {
        return false;
    }

    return demuxer_->seek(params_.start_time, video_stream_->index, AVSEEK_FLAG_FRAME | AVSEEK_FLAG_BACKWARD);
}

bool VideoTranscoder::processVideoFrame(AVFrame* frame)
{
    if (!frame)
    {
        return false;
    }

    stats_.video_frames++;
    frame->pict_type = AV_PICTURE_TYPE_NONE; /// 让编码器自己决定帧类型

    if (progress_callback_)
    {
        progress_callback_(stats_.video_frames, frame->pts, static_cast<int>(output_fps_));
    }

    /// 编码帧
    std::vector<AVPacket*> encoded_packets;
    encoder_->encode_frame(frame, encoded_packets);

    /// 写入编码后的包
    for (auto* pkt : encoded_packets)
    {
        pkt->stream_index = out_video_idx_;
        pkt->pos          = -1;

        int ret = av_interleaved_write_frame(muxer_->getContext(), pkt);
        if (ret < 0)
        {
            PrintErr(ret);
        }

        av_packet_free(&pkt);
        stats_.output_frames++;
    }

    return true;
}

bool VideoTranscoder::flushEncoder()
{
    std::cout << "\n刷新编码器..." << std::endl;

    auto flush_packets = encoder_->flush();

    for (auto* pkt : flush_packets)
    {
        pkt->stream_index = out_video_idx_;
        pkt->pos          = -1;

        int ret = av_interleaved_write_frame(muxer_->getContext(), pkt);
        if (ret < 0)
        {
            PrintErr(ret);
        }

        av_packet_free(&pkt);
        stats_.output_frames++;
    }

    return true;
}

bool VideoTranscoder::transcode()
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

        /// 4. 初始化解码器
        if (!initDecoder())
        {
            return false;
        }

        /// 5. 初始化编码器
        if (!initEncoder())
        {
            return false;
        }

        //////////////////////////////////////////////////////////////////

        /// 6. 写入文件头
        int ret = muxer_->writeHeader();
        CHECK_ERR(ret);

        muxer_->dumpInfo();

        /// 7. 计算PTS
        if (!calculatePTS())
        {
            return false;
        }

        /// 8. 定位到开始时间
        if (!seekToStart())
        {
            std::cerr << "定位失败" << std::endl;
            return false;
        }

        /// 9. 读取并处理数据包
        PacketWrapper         pkt;
        std::vector<AVFrame*> decoded_frames;
        bool                  continue_transcode = true;

        std::cout << "\n开始转码 " << params_.start_time << " ~ " << params_.end_time << " 秒..." << std::endl;

        while (continue_transcode)
        {
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

            stats_.input_packets++;

            /// 处理视频包
            if (video_stream_ && pkt->stream_index == video_stream_->index && params_.transcode_video)
            {
                /// 检查是否超出结束时间
                if (pkt->pts > video_end_pts_)
                {
                    std::cout << "\n达到结束时间" << std::endl;
                    pkt.unref();
                    break;
                }

                /// 解码
                decoded_frames.clear();
                decoder_->decode_packet(pkt, decoded_frames);

                /// 处理解码后的帧
                for (auto* frame : decoded_frames)
                {
                    processVideoFrame(frame);
                    av_frame_free(&frame);
                }
            }
            /// 音频包直通
            else if (audio_stream_ && pkt->stream_index == audio_stream_->index && params_.transcode_audio)
            {
                stats_.audio_packets++;

                /// 计算音频PTS偏移
                if (audio_stream_->time_base.num > 0 && audio_start_pts_ == 0)
                {
                    double t = static_cast<double>(audio_stream_->time_base.den) /
                            static_cast<double>(audio_stream_->time_base.num);
                    audio_start_pts_ = params_.start_time * t;
                }

                muxer_->writePacket(pkt, pkt->stream_index, out_audio_idx_, audio_stream_->time_base, audio_start_pts_);
            }

            pkt.unref();
        }

        /// 10. 刷新解码器
        decoded_frames.clear();
        decoder_->flush(decoded_frames);
        for (auto* frame : decoded_frames)
        {
            processVideoFrame(frame);
            av_frame_free(&frame);
        }

        /// 11. 刷新编码器
        flushEncoder();

        /// 12. 写入文件尾
        ret = muxer_->writeTrailer();
        CHECK_ERR(ret);

        /// 打印统计信息
        std::cout << "\n\n========== 转码完成 ==========" << std::endl;
        std::cout << "输出文件: " << params_.output_file << std::endl;
        std::cout << "输入包数: " << stats_.input_packets << std::endl;
        std::cout << "输出帧数: " << stats_.output_frames << std::endl;
        std::cout << "视频帧数: " << stats_.video_frames << std::endl;
        if (params_.transcode_audio)
        {
            std::cout << "音频包数: " << stats_.audio_packets << std::endl;
        }
        std::cout << "转码时长: " << stats_.duration << " 秒" << std::endl;

        return true;
    }
    catch (const std::exception& e)
    {
        std::cerr << "转码错误: " << e.what() << std::endl;
        return false;
    }
}
