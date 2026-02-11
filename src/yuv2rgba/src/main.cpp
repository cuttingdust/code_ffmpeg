#include <iostream>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
}
#include <fstream>


#define YUV_FILE  ".\\assert\\400_300_25.yuv"
#define RGBA_FILE ".\\assert\\800_600_25.rgba"

int main(int argc, char *argv[])
{
    // std::cout << "FFmpeg version: " << av_version_info() << std::endl;
    // std::cout << "FFmpeg configuration: " << avutil_configuration() << std::endl;
    // std::cout << "FFmpeg license: " << avutil_license() << std::endl;

    /// ffmpeg -i test.mp4 -s 400x300 400_300_25.yuv
    /// 400x300 YUV 转 RGBA 800x600 并存到文件

    int width      = 400;
    int height     = 300;
    int rgb_width  = 800;
    int rgb_height = 600;

    /// YUV420P 平面存储 yyyy uu vv
    unsigned char *yuv[3]          = { 0 };
    int            yuv_linesize[3] = { width, width / 2, width / 2 };
    yuv[0]                         = new unsigned char[width * height];     /// Y
    yuv[1]                         = new unsigned char[width * height / 4]; /// U
    yuv[2]                         = new unsigned char[width * height / 4]; /// V


    /// RGBA交叉存储 rgba rgba
    unsigned char *rgba          = new unsigned char[rgb_width * rgb_height * 4];
    int            rgba_linesize = rgb_width * 4;

    std::ifstream ifs;
    ifs.open(YUV_FILE, std::ios::binary);
    if (!ifs)
    {
        std::cerr << "open " << YUV_FILE << " failed!" << std::endl;
        return -1;
    }

    std::ofstream ofs;
    ofs.open(RGBA_FILE, std::ios::binary);
    if (!ofs)
    {
        std::cerr << "open " << RGBA_FILE << " failed!" << std::endl;
        return -1;
    }

    //////////////////////////////////////////////////////////////////

    SwsContext *yuv2rgb = nullptr;
    for (;;)
    {
        /// 读取YUV帧
        ifs.read((char *)yuv[0], width * height);
        ifs.read((char *)yuv[1], width * height / 4);
        ifs.read((char *)yuv[2], width * height / 4);
        if (ifs.gcount() == 0)
            break;

        /// YUV转RGBA
        /// 上下文件创建和获取
        yuv2rgb = sws_getCachedContext(yuv2rgb,               /// 转换上下文，NULL新创建，非NULL判断与现有参数是否一致，
                                                              /// 一致直接返回，不一致先清理当前然后再创建
                                       width, height,         /// 输入宽高
                                       AV_PIX_FMT_YUV420P,    /// 输入像素格式
                                       rgb_width, rgb_height, /// 输出的宽高
                                       AV_PIX_FMT_RGBA,       /// 输出的像素格式
                                       SWS_BILINEAR,          /// 选择支持变化的算法，双线性插值
                                       0, 0, 0                /// 过滤器参数
        );
        if (!yuv2rgb)
        {
            std::cerr << "sws_getCachedContext failed!" << std::endl;
            return -1;
        }

        unsigned char *data[1];
        data[0]      = rgba;
        int lines[1] = { rgba_linesize };
        int re       = sws_scale(yuv2rgb,
                                 yuv,          /// 输入数据
                                 yuv_linesize, /// 输入数据行字节数
                                 0,
                                 height, /// 输入高度
                                 data,   /// 输出数据
                                 lines);
        std::cout << re << " " << std::flush;
        ofs.write((char *)rgba, rgb_width * rgb_height * 4);
    }

    delete yuv[0];
    delete yuv[1];
    delete yuv[2];
    delete rgba;

    getchar();
    return 0;
}
