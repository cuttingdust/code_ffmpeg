#include "AVConst.h"
#include "EncoderConfig.h"
#include "FileWriter.h"
#include "FrameGenerator.h"
#include "NALAnalyzer.h"
#include "VideoEncoder.h"


/// ==================== 主程序 ====================
int main(int argc, char* argv[])
{
    setlocale(LC_ALL, "zh_CN.UTF-8");

    try
    {
        /// 解析命令行参数
        AVCodecID   codec_id = AV_CODEC_ID_H264;
        std::string filename = "output";

        if (argc > 1)
        {
            std::string codec = argv[1];
            if (codec == "h265" || codec == "hevc")
            {
                codec_id = AV_CODEC_ID_HEVC;
                filename += "_h265";
            }
            else
            {
                filename += "_h264";
            }
        }
        else
        {
            filename += "_h264";
        }

        std::string extension = (codec_id == AV_CODEC_ID_H264) ? ".h264" : ".h265";
        filename += extension;

        std::cout << "\n========== 视频编码器测试程序 ==========" << std::endl;
        std::cout << "输出文件: " << filename << std::endl;
        std::cout << "=========================================\n" << std::endl;

        /// 配置编码器
        EncoderConfig config;
        config.width     = 400;
        config.height    = 300;
        config.framerate = 25;
        config.bitrate   = 400000;
        config.gop_size  = 12;

        /// H264特定配置
        config.h264.crf       = 23;
        config.h264.preset    = "ultrafast";
        config.h264.profile   = "baseline";
        config.h264.tune      = "";    /// 不使用zerolatency以允许IDR
        config.h264.force_idr = true;  /// 强制IDR帧
        config.h264.open_gop  = false; /// 关闭Open GOP

        config.print();

        /// 创建编码器
        VideoEncoder encoder(codec_id, config);

        //. 创建测试帧生成器
        FrameGenerator frame_gen(config.width, config.height, config.pix_fmt);

        /// 创建文件写入器
        FileWriter writer(filename);

        /// 创建NALU分析器
        NALAnalyzer nal_analyzer;

        /// 编码统计
        int       frame_count    = 0;
        int       keyframe_count = 0;
        const int TOTAL_FRAMES   = 250;

        std::cout << "\n开始编码 " << TOTAL_FRAMES << " 帧...\n" << std::endl;

        /// 编码循环
        for (int i = 0; i < TOTAL_FRAMES; i++)
        {
            /// 生成测试帧
            FrameWrapper frame = frame_gen.generate_frame(i);

            /// 编码
            std::vector<AVPacket*> packets;
            encoder.encode_frame(frame, packets);

            /// 处理输出包
            for (auto pkt : packets)
            {
                frame_count++;

                bool is_keyframe = (pkt->flags & AV_PKT_FLAG_KEY);
                if (is_keyframe)
                    keyframe_count++;

                /// 打印帧信息
                std::cout << (is_keyframe ? "[关键帧] " : "         ") << "帧 " << std::setw(3) << frame_count
                          << " (PTS:" << std::setw(3) << pkt->pts << ") 大小:" << std::setw(6) << pkt->size << " 字节";

                if (is_keyframe)
                {
                    std::cout << " [I帧]";
                }
                std::cout << " NALU类型: ";

                /// 分析NALU
                auto nals = nal_analyzer.analyze(pkt);
                nal_analyzer.print_nal_summary(nals);

                /// 写入文件
                writer.write_packet(pkt);

                /// 清理
                av_packet_free(&pkt);
            }
        }

        /// 刷新编码器
        std::cout << "\n刷新编码器缓冲区..." << std::endl;
        auto flush_packets = encoder.flush();
        for (auto pkt : flush_packets)
        {
            frame_count++;
            if (pkt->flags & AV_PKT_FLAG_KEY)
                keyframe_count++;

            std::cout << "刷新帧 " << frame_count << " 大小: " << pkt->size << " 字节" << std::endl;

            writer.write_packet(pkt);
            av_packet_free(&pkt);
        }

        /// 打印统计信息
        std::cout << "\n========== 编码完成 ==========" << std::endl;
        std::cout << "总帧数: " << frame_count << std::endl;
        std::cout << "关键帧数: " << keyframe_count << std::endl;
        std::cout << "关键帧间隔: " << (frame_count > 0 ? frame_count / keyframe_count : 0) << " 帧" << std::endl;
        std::cout << "输出文件: " << writer.get_filename() << std::endl;
        std::cout << "文件大小: " << writer.get_size() / 1024 << " KB" << std::endl;
        std::cout << "平均码率: " << (writer.get_size() * 8 / 1000) / 10 << " kbps" << std::endl;
        std::cout << "==============================\n" << std::endl;
    }
    catch (const std::exception& e)
    {
        std::cerr << "错误: " << e.what() << std::endl;
        return -1;
    }

    return 0;
}
