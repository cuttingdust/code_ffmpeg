#include "XAudioDecodeTask.h"
#include "XAudioPlayTask.h"
#include "XDemuxTask.h"

#include <cstdio>
#include <filesystem>
#include <iostream>
#include <memory>

namespace
{

auto resolveMediaPath() -> const char*
{
    constexpr const char* candidates[] = { "assert/output.mp4", "assert/v1080.mp4" };
    for (const char* path : candidates)
    {
        if (std::filesystem::exists(path))
        {
            return path;
        }
    }
    return candidates[0];
}

} // namespace

int main()
{
    setlocale(LC_ALL, "zh_CN.UTF-8");

    const char* media_path = resolveMediaPath();
    std::cout << "media: " << media_path << std::endl;

    auto demux       = XDemuxTask::create();
    auto audio_dec   = XAudioDecodeTask::create();
    auto audio_play  = XAudioPlayTask::create();

    demux->setName("Demux");
    audio_dec->setName("AudioDecode");
    audio_play->setName("AudioPlay");

    demux->setAudioNext(audio_dec);
    audio_dec->setNext(audio_play);

    demux->setMaxQueueSize(500);
    audio_dec->setMaxQueueSize(500);
    audio_play->setMaxQueueSize(200);

    demux->setIdleTimeoutMs(0);
    audio_dec->setIdleTimeoutMs(0);
    audio_play->setIdleTimeoutMs(0);

    if (!demux->open(media_path))
    {
        std::cerr << "open failed: " << media_path << std::endl;
        std::cerr << "hint: run from out/bin.x64, copy v1080.mp4 -> assert/output.mp4" << std::endl;
        return -1;
    }

    AVStream* audio_stream = demux->getAudioStream();
    if (!audio_stream)
    {
        std::cerr << "no audio stream in: " << media_path << std::endl;
        return -1;
    }

    if (!audio_dec->initDecoder(audio_stream))
    {
        std::cerr << "initDecoder failed" << std::endl;
        return -1;
    }

    if (!audio_play->openFromDecoder(audio_dec->getDecoder()))
    {
        std::cerr << "openFromDecoder failed" << std::endl;
        return -1;
    }

    audio_play->setVolume(1.0);
    audio_play->setSpeed(1.0);

    std::cout << "duration: " << demux->getDuration() << " s" << std::endl;
    std::cout << "starting audio pipeline..." << std::endl;

    audio_play->start();
    audio_dec->start();
    demux->start();

    demux->wait();
    audio_dec->wait();
    audio_play->wait();

    const auto dec_stats  = audio_dec->getStats();
    const auto demux_stats = demux->getStats();

    std::cout << "\n========== done ==========" << std::endl;
    std::cout << "demux audio packets: " << demux_stats.audio_packets << std::endl;
    std::cout << "decoded frames:      " << dec_stats.frames_decoded << std::endl;
    std::cout << "press Enter to exit..." << std::endl;
    std::getchar();

    demux->close();
    return 0;
}
