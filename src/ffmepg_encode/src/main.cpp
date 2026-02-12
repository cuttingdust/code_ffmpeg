#include <iostream>

extern "C" {
#include <libavcodec/avcodec.h>
}

int main(int argc, char *argv[])
{
    /// 1 找到编码器  AV_CODEC_ID_HEVC(H265)
    auto codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!codec)
    {
        std::cerr << "codec not find!" << std::endl;
        return -1;
    }

    /// 2 编码上下文
    auto c = avcodec_alloc_context3(codec);
    if (!c)
    {
        std::cerr << "avcodec_alloc_context3 failed!" << std::endl;
        return -1;
    }

    ///3 设定上下文参数
    c->width  = 400; /// 视频宽高
    c->height = 300;

    /// 帧时间戳的时间单位  pts*time_base = 播放时间（秒）
    c->time_base = { .num = 1, .den = 25 }; /// 分数 1/25

    c->pix_fmt      = AV_PIX_FMT_YUV420P; /// 元数据像素格式，与编码算法相关
    c->thread_count = 16;                 /// 编码线程数，可以通过调用系统接口获取cpu核心数量

    ///4 打开编码上下文
    int re = avcodec_open2(c, codec, NULL);
    if (re != 0)
    {
        char buf[1024] = { 0 };
        av_strerror(re, buf, sizeof(buf) - 1);
        std::cerr << "avcodec_open2 failed!" << buf << std::endl;
        return -1;
    }
    std::cout << "avcodec_open2 success!" << std::endl;

    /// 释放编码器上下文
    avcodec_free_context(&c);

    getchar();
    return 0;
}
