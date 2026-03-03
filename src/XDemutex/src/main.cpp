#include <iostream>
#include <memory>

#include "Demuxer.h"
#include "Muxer.h"
#include "PacketWrapper.h"


int main(int argc, char* argv[])
{
    setlocale(LC_ALL, "zh_CN.UTF-8");

    std::cout << "FFmpeg version: " << av_version_info() << std::endl;
    std::cout << "==========================================" << std::endl;

    try
    {
        // ==================== 第一部分：打开输入文件 ====================
        std::string in_url  = R"(.\assert\v1080.mp4)";
        std::string out_url = R"(.\assert\test_mux_cut.mp4)";

        double begin_sec = 10.0; // 截取开始时间
        double end_sec   = 20.0; // 截取结束时间

        // 创建解封装器
        Demuxer demuxer(in_url);
        if (!demuxer.open())
        {
            std::cerr << "无法打开输入文件" << std::endl;
            return -1;
        }

        // 打印输入文件信息
        demuxer.dumpInfo();

        // 获取输入流
        AVStream* video_stream = demuxer.getVideoStream();
        AVStream* audio_stream = demuxer.getAudioStream();

        if (!video_stream)
        {
            std::cerr << "未找到视频流" << std::endl;
            return -1;
        }

        // ==================== 第二部分：创建输出文件 ====================

        // 创建封装器
        Muxer muxer(out_url);
        if (!muxer.open())
        {
            std::cerr << "无法创建输出文件" << std::endl;
            return -1;
        }

        // 添加输出流
        int out_video_idx = muxer.addVideoStream(video_stream);
        int out_audio_idx = -1;
        if (audio_stream)
        {
            out_audio_idx = muxer.addAudioStream(audio_stream);
        }

        // 写入文件头
        int ret = muxer.writeHeader();
        CHECK_ERR(ret);

        // 打印输出文件信息
        muxer.dumpInfo();

        // ==================== 第三部分：计算截取时间 ====================

        // 计算视频开始和结束的 PTS
        int64_t begin_pts       = 0;
        int64_t end_pts         = 0;
        int64_t begin_audio_pts = 0;

        if (video_stream && video_stream->time_base.num > 0)
        {
            double t  = (double)video_stream->time_base.den / (double)video_stream->time_base.num;
            begin_pts = begin_sec * t;
            end_pts   = end_sec * t;
            std::cout << "\n视频时间基: " << video_stream->time_base.num << "/" << video_stream->time_base.den
                      << ", 1秒 = " << t << " 单位" << std::endl;
            std::cout << "开始 PTS: " << begin_pts << ", 结束 PTS: " << end_pts << std::endl;
        }

        if (audio_stream && audio_stream->time_base.num > 0)
        {
            double t        = (double)audio_stream->time_base.den / (double)audio_stream->time_base.num;
            begin_audio_pts = begin_sec * t;
            std::cout << "音频开始 PTS: " << begin_audio_pts << std::endl;
        }

        // 定位到开始时间
        if (video_stream)
        {
            bool seek_ret = demuxer.seek(begin_sec, video_stream->index, AVSEEK_FLAG_FRAME | AVSEEK_FLAG_BACKWARD);
            if (!seek_ret)
            {
                std::cerr << "定位失败" << std::endl;
            }
        }

        // ==================== 第四部分：读取并写入数据包 ====================

        // 使用 PacketWrapper 自动管理 AVPacket 的生命周期
        PacketWrapper pkt;
        int           video_frame_count = 0;
        int           audio_frame_count = 0;

        std::cout << "\n开始截取 " << begin_sec << " ~ " << end_sec << " 秒..." << std::endl;

        while (true)
        {
            // 读取数据包
            ret = demuxer.readPacket(pkt);

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

            // 获取输入流
            AVStream* in_stream = demuxer.getStream(pkt->stream_index);
            if (!in_stream)
            {
                pkt.unref();
                continue;
            }

            long long offset_pts       = 0;
            int       out_stream_index = -1;

            // 处理视频包
            if (video_stream && pkt->stream_index == video_stream->index)
            {
                video_frame_count++;

                // 显示进度
                std::cout << "\r视频包 #" << video_frame_count << " PTS:" << pkt->pts << " 大小:" << pkt->size
                          << " 关键帧:" << ((pkt->flags & AV_PKT_FLAG_KEY) ? "是" : "否") << std::flush;

                // 检查是否超出结束时间
                if (pkt->pts > end_pts)
                {
                    std::cout << "\n达到结束时间" << std::endl;
                    pkt.unref();
                    break;
                }

                out_stream_index = out_video_idx;
                offset_pts       = begin_pts;
            }
            // 处理音频包
            else if (audio_stream && pkt->stream_index == audio_stream->index)
            {
                audio_frame_count++;
                out_stream_index = out_audio_idx;
                offset_pts       = begin_audio_pts;
            }
            else
            {
                // 不是要处理的流
                pkt.unref();
                continue;
            }

            // 写入数据包（Muxer会自动处理时基转换和PTS偏移）
            if (out_stream_index >= 0)
            {
                ret = muxer.writePacket(pkt, pkt->stream_index, out_stream_index, in_stream->time_base, offset_pts);
                if (ret < 0)
                {
                    PrintErr(ret);
                }
            }

            // PacketWrapper 会自动处理 unref，但为了保险，还是调用一下
            pkt.unref();
        }

        // ==================== 第五部分：收尾和清理 ====================

        // 写入文件尾
        ret = muxer.writeTrailer();
        CHECK_ERR(ret);

        // 打印统计信息
        std::cout << "\n\n========== 截取完成 ==========" << std::endl;
        std::cout << "输出文件: " << out_url << std::endl;
        std::cout << "视频包数: " << video_frame_count << std::endl;
        std::cout << "音频包数: " << audio_frame_count << std::endl;

        // PacketWrapper 会在析构时自动释放 pkt，不需要手动 av_packet_free
    }
    catch (const std::exception& e)
    {
        std::cerr << "错误: " << e.what() << std::endl;
        return -1;
    }

    std::cout << "\n按回车键退出..." << std::endl;
    getchar();
    return 0;
}
