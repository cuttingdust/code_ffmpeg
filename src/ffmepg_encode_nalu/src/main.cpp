#include <iostream>
#include <fstream>
#include <iomanip>
#include <vector>
#include <string>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libavutil/imgutils.h>
#include <libavutil/avutil.h>
}

/// ==================== 工具函数声明 ====================

/// 打印编码器当前参数
void print_codec_params(AVCodecContext *c);

/// 打印AVDictionary中的所有参数
void print_dict(const AVDictionary *dict, const std::string &title);

/// 安全设置编码器参数，带错误处理
int set_dict_param(AVDictionary **opts, const char *key, const char *value, const std::string &codec_name);

/// 获取NALU类型名称的字符串表示
const char *get_nal_type_name(int nal_type);

/// 解析并打印AVPacket中的所有NALU
void analyze_nal_units(const AVPacket *pkt, int frame_number);

/// 生成测试用的YUV420P帧数据
void fill_test_yuv_frame(AVFrame *frame, int width, int height, int frame_index);

/// ==================== 主函数 ====================

int main(int argc, char *argv[])
{
    /// 设置本地化，支持中文输出
    setlocale(LC_ALL, "zh_CN.UTF-8");

    /// ==================== 1. 解析命令行参数 ====================
    std::string filename = "400_300_25_preset";
    AVCodecID   codec_id = AV_CODEC_ID_H264;

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

    std::string codec_display_name = (codec_id == AV_CODEC_ID_H264) ? "H264" : "H265";
    std::string extension          = (codec_id == AV_CODEC_ID_H264) ? ".h264" : ".h265";
    filename += extension;

    std::cout << "\n========== 视频编码器测试程序 ==========" << std::endl;
    std::cout << "使用编码器: " << codec_display_name << std::endl;
    std::cout << "输出文件: " << filename << std::endl;
    std::cout << "=========================================\n" << std::endl;

    /// 打开输出文件
    std::ofstream ofs;
    ofs.open(filename, std::ios::binary);
    if (!ofs.is_open())
    {
        std::cerr << "错误: 无法创建输出文件!" << std::endl;
        return -1;
    }

    /// ==================== 2. 查找编码器 ====================
    const AVCodec *codec = avcodec_find_encoder(codec_id);
    if (!codec)
    {
        std::cerr << "错误: 找不到编码器 " << codec_display_name << std::endl;
        return -1;
    }

    std::cout << "编码器名称: " << codec->name << std::endl;
    std::cout << "编码器描述: " << codec->long_name << std::endl;

    /// ==================== 3. 创建编码器上下文 ====================
    AVCodecContext *c = avcodec_alloc_context3(codec);
    if (!c)
    {
        std::cerr << "错误: avcodec_alloc_context3 失败!" << std::endl;
        return -1;
    }

    /// ==================== 4. 设置基础编码参数 ====================
    c->width        = 400;                     /// 视频宽度
    c->height       = 300;                     /// 视频高度
    c->time_base    = { .num = 1, .den = 25 }; /// 时间基: 1/25秒每帧
    c->framerate    = { .num = 25, .den = 1 }; /// 帧率: 25fps
    c->pix_fmt      = AV_PIX_FMT_YUV420P;      /// 像素格式: YUV420P
    c->thread_count = 16;                      /// 编码线程数
    c->max_b_frames = 0;                       /// B帧数量: 0（低延迟模式）
    c->bit_rate     = 400000;                  /// 目标比特率: 400kbps
    c->gop_size     = 12;                      /// 关键帧间隔: 每12帧一个I帧

    /// ==================== 5. 设置码率控制参数 ====================
    int br = c->bit_rate;

    /// VBV (Video Buffering Verifier) 缓冲区设置
    /// 用于约束码率波动，防止缓冲区上溢/下溢
    c->rc_max_rate    = br;     /// 最大比特率
    c->rc_buffer_size = br * 2; /// 缓冲区大小（通常为比特率的2倍）

    /// 注意: 不设置 rc_min_rate，允许码率低于目标值（ABR模式）

    /// ==================== 6. 设置编码器特定参数 ====================
    AVDictionary *opts       = NULL;
    int           set_count  = 0;
    int           fail_count = 0;

    if (codec_id == AV_CODEC_ID_H264)
    {
        std::cout << "\n--- 设置H264特定参数 ---" << std::endl;

        /// 使用CRF (Constant Rate Factor) 恒定质量模式
        /// CRF范围: 0-51，数值越小质量越高
        /// - 18: 视觉无损
        /// - 23: 默认值，良好质量
        /// - 28: 较小文件，适合移动设备
        if (set_dict_param(&opts, "crf", "23", codec_display_name) >= 0)
            set_count++;
        else
            fail_count++;

        /// 预设编码速度/压缩率平衡
        /// ultrafast: 最快编码，最大文件
        /// medium: 默认平衡
        /// veryslow: 最慢编码，最小文件
        if (set_dict_param(&opts, "preset", "ultrafast", codec_display_name) >= 0)
            set_count++;
        else
            fail_count++;

        // /// 调优参数
        // /// zerolatency: 零延迟模式，适合实时通信
        // /// film: 针对电影内容优化
        // /// animation: 针对动画内容优化
        // if (set_dict_param(&opts, "tune", "zerolatency", codec_display_name) >= 0)
        //     set_count++;
        // else
        //     fail_count++;

        /// Profile设置
        /// baseline: 基础档次，兼容性最好
        /// main: 主要档次，平衡兼容性和压缩率
        /// high: 高级档次，压缩率最高
        if (set_dict_param(&opts, "profile", "baseline", codec_display_name) >= 0)
            set_count++;
        else
            fail_count++;

        // 强制使用IDR帧的关键参数！
        if (set_dict_param(&opts, "open-gop", "0", codec_display_name) >= 0) // 禁用Open GOP
            set_count++;
        else
            fail_count++;

        if (set_dict_param(&opts, "keyint", "12", codec_display_name) >= 0) // 强制IDR间隔
            set_count++;
        else
            fail_count++;

        if (set_dict_param(&opts, "min-keyint", "12", codec_display_name) >= 0)
            set_count++;
        else
            fail_count++;
    }
    else if (codec_id == AV_CODEC_ID_HEVC)
    {
        std::cout << "\n--- 设置H265特定参数 ---" << std::endl;

        /// x265参数通过x265-params传递
        std::string x265_params = "preset=ultrafast:";
        x265_params += "tu-intra-depth=4:"; /// 变换单元深度
        x265_params += "crf=23:";           /// 恒定质量值
        x265_params += "bframes=0";         /// 0 B帧

        if (set_dict_param(&opts, "x265-params", x265_params.c_str(), codec_display_name) >= 0)
            set_count++;
        else
            fail_count++;
    }

    std::cout << "参数设置统计: 成功 " << set_count << " 个, 失败 " << fail_count << " 个" << std::endl;

    /// 打印所有即将设置的参数
    if (opts)
    {
        print_dict(opts, "即将设置的参数");
    }

    /// ==================== 7. 打开编码器 ====================
    std::cout << "\n正在打开编码器..." << std::endl;
    int ret = avcodec_open2(c, codec, &opts);

    /// 检查未使用的参数（编码器不支持的参数）
    if (opts)
    {
        const AVDictionaryEntry *entry      = nullptr;
        bool                     has_unused = false;
        while ((entry = av_dict_get(opts, "", entry, AV_DICT_IGNORE_SUFFIX)))
        {
            if (!has_unused)
            {
                std::cout << "\n警告: 以下参数未被编码器接受:" << std::endl;
                has_unused = true;
            }
            std::cout << "  " << entry->key << " = " << entry->value << " (未使用)" << std::endl;
        }
    }

    av_dict_free(&opts); /// 释放参数字典

    if (ret != 0)
    {
        char buf[1024] = { 0 };
        av_strerror(ret, buf, sizeof(buf) - 1);
        std::cerr << "错误: avcodec_open2 失败! " << buf << std::endl;
        return -1;
    }
    std::cout << "编码器打开成功!" << std::endl;

    /// 打印编码器最终使用的参数
    print_codec_params(c);

    /// ==================== 8. 创建AVFrame（存储原始YUV数据） ====================
    AVFrame *frame = av_frame_alloc();
    if (!frame)
    {
        std::cerr << "错误: av_frame_alloc 失败!" << std::endl;
        return -1;
    }

    frame->width  = c->width;
    frame->height = c->height;
    frame->format = c->pix_fmt;

    ret = av_frame_get_buffer(frame, 0); /// 分配帧数据缓冲区
    if (ret != 0)
    {
        char buf[1024] = { 0 };
        av_strerror(ret, buf, sizeof(buf) - 1);
        std::cerr << "错误: av_frame_get_buffer 失败! " << buf << std::endl;
        return -1;
    }

    /// ==================== 9. 创建AVPacket（存储编码后的数据） ====================
    AVPacket *pkt = av_packet_alloc();
    if (!pkt)
    {
        std::cerr << "错误: av_packet_alloc 失败!" << std::endl;
        return -1;
    }

    /// ==================== 10. 开始编码循环 ====================
    int       frame_count    = 0;
    int       keyframe_count = 0;
    const int TOTAL_FRAMES   = 250; /// 10秒视频 (25fps * 10)

    std::cout << "\n开始编码 " << TOTAL_FRAMES << " 帧...\n" << std::endl;

    for (int i = 0; i < TOTAL_FRAMES; i++)
    {
        /// 10.1 生成测试用的YUV帧数据
        fill_test_yuv_frame(frame, c->width, c->height, i);

        /// 10.2 设置PTS（显示时间戳）
        frame->pts = i;

        /// 10.3 设置帧类型提示（可选，编码器可能忽略）
        if (i % c->gop_size == 0)
        {
            frame->pict_type = AV_PICTURE_TYPE_I; /// 提示编码器此帧适合做I帧
        }
        else
        {
            frame->pict_type = AV_PICTURE_TYPE_NONE; /// 让编码器自动选择
        }

        /// 10.4 发送原始帧到编码器
        ret = avcodec_send_frame(c, frame);
        if (ret < 0)
        {
            char buf[1024] = { 0 };
            av_strerror(ret, buf, sizeof(buf) - 1);
            std::cerr << "错误: avcodec_send_frame 失败 (帧 " << i << ")! " << buf << std::endl;
            break;
        }

        /// 10.5 接收编码后的数据包
        while (true)
        {
            ret = avcodec_receive_packet(c, pkt);

            if (ret == AVERROR(EAGAIN))
            {
                /// 编码器需要更多帧才能输出
                break;
            }
            else if (ret == AVERROR_EOF)
            {
                /// 编码器已刷新，没有更多数据
                break;
            }
            else if (ret < 0)
            {
                char buf[1024] = { 0 };
                av_strerror(ret, buf, sizeof(buf) - 1);
                std::cerr << "错误: avcodec_receive_packet 失败! " << buf << std::endl;
                break;
            }

            frame_count++;

            /// 统计关键帧
            if (pkt->flags & AV_PKT_FLAG_KEY)
            {
                keyframe_count++;
                std::cout << "[关键帧] ";
            }

            /// 打印帧信息
            std::cout << "帧 " << std::setw(3) << frame_count << " (PTS:" << std::setw(3) << pkt->pts
                      << ") 大小:" << std::setw(6) << pkt->size << " 字节";

            if (pkt->flags & AV_PKT_FLAG_KEY)
            {
                std::cout << " [I帧]";
            }
            std::cout << " NALU类型: ";

            /// 分析并打印包内的所有NALU
            analyze_nal_units(pkt, frame_count);

            /// 写入文件
            ofs.write((char *)pkt->data, pkt->size);

            /// 释放包引用（重要！避免内存泄漏）
            av_packet_unref(pkt);
        }
    }

    /// ==================== 11. 刷新编码器缓冲区 ====================
    std::cout << "\n刷新编码器缓冲区..." << std::endl;

    /// 发送NULL帧，通知编码器结束
    avcodec_send_frame(c, nullptr);

    /// 接收所有剩余的编码数据
    while (true)
    {
        ret = avcodec_receive_packet(c, pkt);
        if (ret == AVERROR_EOF)
            break;
        if (ret < 0)
            break;

        frame_count++;
        if (pkt->flags & AV_PKT_FLAG_KEY)
        {
            keyframe_count++;
            std::cout << "[关键帧] ";
        }
        std::cout << "刷新帧 " << frame_count << " 大小: " << pkt->size << " 字节" << std::endl;

        ofs.write((char *)pkt->data, pkt->size);
        av_packet_unref(pkt);
    }

    /// ==================== 12. 获取文件大小 ====================
    ofs.seekp(0, std::ios::end);
    std::streampos file_size = ofs.tellp();
    ofs.close();

    /// ==================== 13. 清理资源 ====================
    av_packet_free(&pkt);
    av_frame_free(&frame);
    avcodec_free_context(&c);

    /// ==================== 14. 打印统计信息 ====================
    std::cout << "\n========== 编码完成 ==========" << std::endl;
    std::cout << "总帧数: " << frame_count << std::endl;
    std::cout << "关键帧数: " << keyframe_count << std::endl;
    std::cout << "关键帧间隔: " << (frame_count > 0 ? frame_count / keyframe_count : 0) << " 帧" << std::endl;
    std::cout << "输出文件: " << filename << std::endl;
    std::cout << "文件大小: " << file_size / 1024 << " KB" << std::endl;
    std::cout << "平均码率: " << (file_size * 8 / 1000) / 10 << " kbps" << std::endl; /// 10秒视频
    std::cout << "==============================\n" << std::endl;

    return 0;
}

/// ==================== 工具函数实现 ====================

/**
 * @brief 打印编码器上下文的参数
 * @param c AVCodecContext指针
 */
void print_codec_params(AVCodecContext *c)
{
    std::cout << "\n========== 编码器参数 ==========" << std::endl;
    std::cout << "宽度: " << c->width << std::endl;
    std::cout << "高度: " << c->height << std::endl;
    std::cout << "像素格式: " << av_get_pix_fmt_name(c->pix_fmt) << std::endl;
    std::cout << "时间基: " << c->time_base.num << "/" << c->time_base.den << std::endl;
    std::cout << "帧率: " << av_q2d(c->framerate) << " fps" << std::endl;
    std::cout << "目标比特率: " << c->bit_rate / 1000 << " kbps" << std::endl;
    std::cout << "最大比特率: " << c->rc_max_rate / 1000 << " kbps" << std::endl;
    std::cout << "缓冲区大小: " << c->rc_buffer_size / 1000 << " kbits" << std::endl;
    std::cout << "GOP大小: " << c->gop_size << " 帧" << std::endl;
    std::cout << "最大B帧: " << c->max_b_frames << std::endl;
    std::cout << "线程数: " << c->thread_count << std::endl;
    std::cout << "编码器名称: " << c->codec->name << std::endl;
    std::cout << "================================\n" << std::endl;
}

/**
 * @brief 打印AVDictionary中的所有键值对
 * @param dict AVDictionary指针
 * @param title 标题
 */
void print_dict(const AVDictionary *dict, const std::string &title)
{
    std::cout << title << ":" << std::endl;
    const AVDictionaryEntry *entry = nullptr;
    while ((entry = av_dict_get(dict, "", entry, AV_DICT_IGNORE_SUFFIX)))
    {
        std::cout << "  " << entry->key << " = " << entry->value << std::endl;
    }
}

/**
 * @brief 设置编码器参数，带错误处理和提示
 * @param opts 参数字典指针的指针
 * @param key 参数名
 * @param value 参数值
 * @param codec_name 编码器名称（用于显示）
 * @return 0成功，负值失败
 */
int set_dict_param(AVDictionary **opts, const char *key, const char *value, const std::string &codec_name)
{
    int ret = av_dict_set(opts, key, value, 0);
    if (ret < 0)
    {
        char buf[256] = { 0 };
        av_strerror(ret, buf, sizeof(buf));
        std::cerr << "警告: " << codec_name << " 参数 '" << key << "' 设置失败: " << buf << std::endl;
    }
    else
    {
        std::cout << codec_name << " 参数设置: " << key << " = " << value << std::endl;
    }
    return ret;
}

/**
 * @brief 获取NALU类型的名称
 * @param nal_type NALU类型值
 * @return 类型名称字符串
 */
const char *get_nal_type_name(int nal_type)
{
    switch (nal_type)
    {
        case 1:
            return "非IDR切片";
        case 2:
            return "切片分区A";
        case 3:
            return "切片分区B";
        case 4:
            return "切片分区C";
        case 5:
            return "IDR切片";
        case 6:
            return "SEI";
        case 7:
            return "SPS";
        case 8:
            return "PPS";
        case 9:
            return "访问单元分隔符";
        case 10:
            return "序列结束";
        case 11:
            return "流结束";
        case 12:
            return "填充数据";
        case 13:
            return "序列参数集扩展";
        case 14:
            return "前缀NALU";
        case 15:
            return "子集序列参数集";
        case 16:
            return "深度参数集";
        case 17:
            return "保留";
        case 18:
            return "保留";
        case 19:
            return "切片辅助";
        default:
            return "未知类型";
    }
}

/**
 * @brief 解析AVPacket中的所有NALU并打印
 * @param pkt AVPacket指针
 * @param frame_number 帧号（用于显示）
 * 
 * H.264/H.265码流结构：
 * [起始码][NALU头][NALU数据][起始码][NALU头][NALU数据]...
 * 
 * 起始码格式：
 * - 4字节: 0x00 0x00 0x00 0x01 (常用)
 * - 3字节: 0x00 0x00 0x01 (较少用)
 * 
 * NALU头(1字节):
 * +-------+-------+---------------+
 * | bit7  | bit6-5|    bit4-0     |
 * +-------+-------+---------------+
 * | F-bit | NRI   |  NALU Type    |
 * +-------+-------+---------------+
 */
void analyze_nal_units(const AVPacket *pkt, int frame_number)
{
    if (!pkt || pkt->size < 4)
        return;

    const uint8_t *data      = pkt->data;
    int            size      = pkt->size;
    int            offset    = 0;
    int            nal_count = 0;

    std::vector<int> nal_types;

    while (offset < size - 4)
    {
        /// 查找4字节起始码 (0x00 0x00 0x00 0x01)
        if (data[offset] == 0 && data[offset + 1] == 0 && data[offset + 2] == 0 && data[offset + 3] == 1)
        {
            /// 找到起始码
            int nal_start = offset + 4; /// NALU数据开始位置（跳过起始码）

            if (nal_start < size)
            {
                /// 解析NALU头
                uint8_t nal_head  = data[nal_start];
                int     nal_type  = nal_head & 0x1F;        /// 取低5位：NALU类型
                int     nal_ref   = (nal_head >> 5) & 0x03; /// 取第5-6位：优先级
                int     forbidden = (nal_head >> 7) & 0x01; /// 取第7位：必须为0

                nal_types.push_back(nal_type);
                nal_count++;

                /// 查找下一个起始码，计算当前NALU大小
                int next_offset = nal_start + 1;
                while (next_offset < size - 4)
                {
                    if (data[next_offset] == 0 && data[next_offset + 1] == 0 && data[next_offset + 2] == 0 &&
                        data[next_offset + 3] == 1)
                    {
                        break;
                    }
                    next_offset++;
                }

                int nal_size = next_offset - nal_start;

                /// 打印NALU信息（详细模式）

                std::cout << "\n    NALU[" << nal_count << "]: 类型=" << std::setw(2) << nal_type << " ("
                          << get_nal_type_name(nal_type) << ")"
                          << " 大小=" << nal_size << " 优先级=" << nal_ref;
            }

            offset += 4; /// 跳过当前起始码
        }
        else
        {
            offset++;
        }
    }

    /// 打印NALU类型序列（简洁模式）
    for (size_t i = 0; i < nal_types.size(); i++)
    {
        if (i > 0)
            std::cout << ",";
        std::cout << nal_types[i];
    }
    std::cout << std::endl;
}

/**
 * @brief 生成测试用的YUV420P帧数据
 * @param frame AVFrame指针
 * @param width 宽度
 * @param height 高度
 * @param frame_index 帧索引（用于生成变化的数据）
 * 
 * YUV420P格式说明：
 * - Y平面: 宽度×高度，每个像素1字节
 * - U平面: 宽度/2 × 高度/2，每个像素1字节
 * - V平面: 宽度/2 × 高度/2，每个像素1字节
 */
void fill_test_yuv_frame(AVFrame *frame, int width, int height, int frame_index)
{
    if (!frame || !frame->data[0])
        return;

    /// 生成Y平面数据（亮度）
    /// 使用渐变的水平+垂直条纹，随时间变化
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            /// 公式: x + y + 帧索引*3，产生动态变化的渐变图案
            /// 范围: 0-255，会自动截断
            frame->data[0][y * frame->linesize[0] + x] = (x + y + frame_index * 3) & 0xFF;
        }
    }

    /// 生成U平面数据（色度，蓝色差）
    /// 宽度和高度都是Y平面的一半
    for (int y = 0; y < height / 2; y++)
    {
        for (int x = 0; x < width / 2; x++)
        {
            /// U分量，偏蓝色调，随时间变化
            frame->data[1][y * frame->linesize[1] + x] = (128 + y + frame_index * 2) & 0xFF;
        }
    }

    /// 生成V平面数据（色度，红色差）
    for (int y = 0; y < height / 2; y++)
    {
        for (int x = 0; x < width / 2; x++)
        {
            /// V分量，偏红色调，随时间变化
            frame->data[2][y * frame->linesize[2] + x] = (64 + x + frame_index * 5) & 0xFF;
        }
    }
}
