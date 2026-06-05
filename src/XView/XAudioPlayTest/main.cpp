#include "XAudioPlay.h"

#include <fstream>
#include <iostream>
#include <memory>

int main()
{
    const char *pcm_path = "assert/output.pcm";

    std::ifstream ifs(pcm_path, std::ios::binary);
    if (!ifs)
    {
        std::cerr << "open pcm failed: " << pcm_path << std::endl;
        return -1;
    }

    std::unique_ptr<XAudioPlay> player(XAudioPlay::create());
    if (!player)
    {
        std::cerr << "XAudioPlay::create failed" << std::endl;
        return -1;
    }

    XAudioPlay::Spec spec;
    spec.sample_rate = 44100;
    spec.channels    = 2;
    spec.samples     = 1024;

    if (!player->open(spec,
                      [&ifs](uint8_t *dst, int bytes) -> int
                      {
                          ifs.read(reinterpret_cast<char *>(dst), bytes);
                          return static_cast<int>(ifs.gcount());
                      }))
    {
        std::cerr << "open audio failed: " << player->lastError() << std::endl;
        return -1;
    }

    player->start();
    std::cout << "playing " << pcm_path << ", press Enter to exit..." << std::endl;
    std::cin.get();

    player->pause();
    player->close();
    return 0;
}
