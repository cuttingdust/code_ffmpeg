#include "XAudioPlay.h"

#include "AVLog.h"

#include <SDL.h>

#include <mutex>
#include <string>

namespace
{
    std::once_flag g_sdl_audio_once;
    std::mutex     g_sdl_audio_init_mtx;
    int            g_sdl_audio_ref = 0;

    auto refSdlAudio() -> bool
    {
        std::scoped_lock lock(g_sdl_audio_init_mtx);
        if (g_sdl_audio_ref == 0)
        {
            std::call_once(g_sdl_audio_once,
                           []()
                           {
                               if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0)
                               {
                                   LOGE("SDL_InitSubSystem(AUDIO) failed: " << SDL_GetError());
                               }
                           });
            if ((SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO) == 0)
            {
                return false;
            }
        }
        ++g_sdl_audio_ref;
        return true;
    }

    auto unrefSdlAudio() -> void
    {
        std::scoped_lock lock(g_sdl_audio_init_mtx);
        if (g_sdl_audio_ref > 0)
        {
            --g_sdl_audio_ref;
        }
        if (g_sdl_audio_ref == 0)
        {
            SDL_QuitSubSystem(SDL_INIT_AUDIO);
        }
    }
} // namespace

class XSDLAudioPlay final : public XAudioPlay
{
public:
    XSDLAudioPlay() = default;
    ~XSDLAudioPlay() override
    {
        close();
    }

    auto open(const Spec &spec, FillCallback fill) -> bool override
    {
        close();
        if (!fill)
        {
            last_error_ = "fill callback is null";
            return false;
        }
        if (!refSdlAudio())
        {
            last_error_ = "SDL audio init failed";
            return false;
        }
        sdl_inited_ = true;

        SDL_AudioSpec want{};
        want.freq     = spec.sample_rate;
        want.format   = AUDIO_S16SYS;
        want.channels = static_cast<Uint8>(spec.channels);
        want.samples  = static_cast<Uint16>(spec.samples);
        want.callback = &XSDLAudioPlay::sdlCallback;
        want.userdata = this;

        device_id_ = SDL_OpenAudioDevice(nullptr, 0, &want, &obtained_, SDL_AUDIO_ALLOW_ANY_CHANGE);
        if (device_id_ == 0)
        {
            last_error_ = SDL_GetError();
            LOGE("SDL_OpenAudioDevice failed: " << last_error_);
            close();
            return false;
        }

        spec_    = spec;
        fill_cb_ = std::move(fill);
        opened_  = true;
        LOGI("XAudioPlay opened: " << obtained_.freq << "Hz " << static_cast<int>(obtained_.channels) << "ch");
        return true;
    }

    auto start() -> void override
    {
        if (device_id_ != 0)
        {
            SDL_PauseAudioDevice(device_id_, 0);
            paused_ = false;
        }
    }

    auto pause() -> void override
    {
        if (device_id_ != 0)
        {
            SDL_PauseAudioDevice(device_id_, 1);
            paused_ = true;
        }
    }

    auto close() -> void override
    {
        if (device_id_ != 0)
        {
            SDL_CloseAudioDevice(device_id_);
            device_id_ = 0;
        }
        {
            std::scoped_lock lock(cb_mtx_);
            fill_cb_ = nullptr;
        }
        opened_ = false;
        paused_ = false;
        if (sdl_inited_)
        {
            unrefSdlAudio();
            sdl_inited_ = false;
        }
    }

    auto isOpen() const -> bool override
    {
        return opened_;
    }

    auto lastError() const -> std::string override
    {
        return last_error_;
    }

private:
    static void SDLCALL sdlCallback(void *userdata, Uint8 *stream, int len)
    {
        auto *self = static_cast<XSDLAudioPlay *>(userdata);
        if (!self)
        {
            SDL_memset(stream, 0, len);
            return;
        }

        FillCallback fill;
        {
            std::scoped_lock lock(self->cb_mtx_);
            fill = self->fill_cb_;
        }

        SDL_memset(stream, 0, len);
        if (!fill)
        {
            return;
        }

        const int written = fill(stream, len);
        if (written <= 0)
        {
            SDL_PauseAudioDevice(self->device_id_, 1);
            self->paused_ = true;
        }
    }

    Spec              spec_{};
    FillCallback      fill_cb_;
    SDL_AudioSpec     obtained_{};
    SDL_AudioDeviceID device_id_  = 0;
    bool              opened_     = false;
    bool              paused_     = false;
    bool              sdl_inited_ = false;
    std::string       last_error_;
    std::mutex        cb_mtx_;
};

auto XAudioPlay::create(Backend type) -> XAudioPlay *
{
    switch (type)
    {
        case Backend::SDL:
            return new XSDLAudioPlay();
        default:
            break;
    }
    return nullptr;
}
