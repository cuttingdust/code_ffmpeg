#include "XAudioPlay.h"

#include <chrono>
#include <fstream>
#include <iostream>
#include <memory>
#include <thread>

int main()
{
    setlocale(LC_ALL, "zh_CN.UTF-8");

    const char *pcm_path = "assert/output.pcm";

    std::ifstream ifs(pcm_path, std::ios::binary);
    if (!ifs)
    {
        std::cerr << "open pcm failed: " << pcm_path << std::endl;
        std::cerr << "hint: run from out/bin.x64 or ensure assert/ is copied" << std::endl;
        return -1;
    }

    ifs.seekg(0, std::ios::end);
    const auto file_bytes = static_cast<std::size_t>(ifs.tellg());
    ifs.seekg(0, std::ios::beg);
    if (file_bytes == 0)
    {
        std::cerr << "pcm file is empty: " << pcm_path << std::endl;
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

    if (!player->open(spec))
    {
        std::cerr << "open audio failed: " << player->lastError() << std::endl;
        return -1;
    }

    player->setSpeed(2.0);
    player->setVolume(0.8);

    const auto bytes_per_sec = static_cast<std::size_t>(spec.sample_rate * spec.channels * sizeof(int16_t));
    const auto duration_sec  = static_cast<double>(file_bytes) / static_cast<double>(bytes_per_sec);
    std::cout << "pcm size: " << file_bytes << " bytes, about " << duration_sec << " s" << std::endl;

    unsigned char buf[4096] = {};
    std::size_t   pushed    = 0;
    bool          started   = false;

    while (true)
    {
        ifs.read(reinterpret_cast<char *>(buf), sizeof(buf));
        const int len = static_cast<int>(ifs.gcount());
        if (len <= 0)
        {
            break;
        }

        if (!player->push(buf, len))
        {
            std::cerr << "push pcm failed, queue may be full" << std::endl;
            return -1;
        }
        pushed += static_cast<std::size_t>(len);

        /// 先缓冲一小段再 start，避免设备启动瞬间队列空导致长时间无声
        if (!started && player->queuedBytes() >= sizeof(buf) * 4)
        {
            player->start();
            started = true;
            std::cout << "audio device started, queued " << player->queuedBytes() << " bytes" << std::endl;
        }
    }

    if (!started)
    {
        player->start();
        started = true;
        std::cout << "audio device started, queued " << player->queuedBytes() << " bytes" << std::endl;
    }

    std::cout << "pushed total: " << pushed << " bytes, waiting playback..." << std::endl;

    while (player->queuedBytes() > 0)
    {
        std::cout << "  playing, queued: " << player->queuedBytes() << " bytes\r" << std::flush;
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    std::cout << std::endl << "playback finished, press Enter to exit..." << std::endl;
    std::cin.get();

    player->pause();
    player->close();
    return 0;
}
