#include <iostream>

extern "C" {
#include <libavcodec/avcodec.h>
}

int main(int argc, char *argv[])
{
    std::cout << "FFmpeg version: " << av_version_info() << std::endl;
    std::cout << "FFmpeg configuration: " << avutil_configuration() << std::endl;
    std::cout << "FFmpeg license: " << avutil_license() << std::endl;

    getchar();
    return 0;
}
