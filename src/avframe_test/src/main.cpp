#include <iostream>

extern "C" {
#include <libavcodec/avcodec.h>
}

int main(int argc, char *argv[])
{
    std::cout << "FFmpeg version: " << av_version_info() << std::endl;
    std::cout << "FFmpeg configuration: " << avutil_configuration() << std::endl;
    std::cout << "FFmpeg license: " << avutil_license() << std::endl;

    /// 创建frame对象
    auto frame1 = av_frame_alloc();

    /// 图像参数
    frame1->width  = 401;
    frame1->height = 300;
    frame1->format = AV_PIX_FMT_ARGB;

    /// 分配空间 16字节对齐
    int re = av_frame_get_buffer(frame1, 16); /// 120300
    if (re != 0)
    {
        char buf[1024] = { 0 };
        av_strerror(re, buf, sizeof(buf));
        std::cout << buf << std::endl;
    }
    std::cout << "frame1 linesize[0]=" << frame1->linesize[0] << std::endl; /// 1616

    if (frame1->buf[0])
    {
        std::cout << "frame1 ref count = " << av_buffer_get_ref_count(frame1->buf[0]); /// 线程安全
        std::cout << std::endl;
    }
    auto frame2 = av_frame_alloc();
    av_frame_ref(frame2, frame1);
    std::cout << "frame1 ref count = " << av_buffer_get_ref_count(frame1->buf[0]) << std::endl;
    std::cout << "frame2 ref count = " << av_buffer_get_ref_count(frame2->buf[0]) << std::endl;

    /// 引用计数-1 并将buf清零
    av_frame_unref(frame2);
    std::cout << "av_frame_unref(frame2)" << std::endl;
    std::cout << "frame1 ref count = " << av_buffer_get_ref_count(frame1->buf[0]) << std::endl;


    /// 引用计数为1 直接删除buf空间 引用计数变为0
    av_frame_unref(frame1);

    /// 是否frame对象空间，buf的引用计数减一
    /// buf已经为空，只删除frame对象空间
    av_frame_free(&frame1);
    av_frame_free(&frame2);

    getchar();
    return 0;
}
