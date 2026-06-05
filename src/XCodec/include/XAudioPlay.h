#pragma once

#include "XCodec_Global.h"

#include <cstdint>
#include <functional>
#include <string>

class XCODEC_EXPORT XAudioPlay
{
public:
    enum class Backend
    {
        SDL = 0
    };

    struct Spec
    {
        int sample_rate = 44100;
        int channels    = 2;
        int samples     = 1024;
    };

    using FillCallback = std::function<int(uint8_t *dst, int bytes)>;

    virtual ~XAudioPlay() = default;

    static auto create(Backend type = Backend::SDL) -> XAudioPlay *;

    virtual auto open(const Spec &spec, FillCallback fill) -> bool = 0;
    virtual auto start() -> void                                   = 0;
    virtual auto pause() -> void                                   = 0;
    virtual auto close() -> void                                   = 0;
    virtual auto isOpen() const -> bool                            = 0;
    virtual auto lastError() const -> std::string                  = 0;
};
