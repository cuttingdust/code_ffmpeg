#include "EncoderConfig.h"

void EncoderConfig::print() const
{
    std::cout << "\n========== 编码器配置 ==========" << std::endl;
    std::cout << "分辨率: " << width << "x" << height << std::endl;
    std::cout << "帧率: " << framerate << " fps" << std::endl;
    std::cout << "比特率: " << bitrate / 1000 << " kbps" << std::endl;
    std::cout << "GOP大小: " << gop_size << " 帧" << std::endl;
    std::cout << "最大B帧: " << max_b_frames << std::endl;
    std::cout << "线程数: " << thread_count << std::endl;
    std::cout << "================================\n" << std::endl;
}
