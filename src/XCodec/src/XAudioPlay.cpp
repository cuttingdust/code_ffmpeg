#include "XAudioPlay.h"

#include "AVLog.h"

#include <SDL.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <mutex>
#include <string>

/// SDL 音频子系统全局引用计数（与 XSDL 的 VIDEO 子系统独立管理）。
/// 首个播放器 open 时 SDL_InitSubSystem(SDL_INIT_AUDIO)；
/// 最后一个播放器 close 且引用归零时 SDL_QuitSubSystem(SDL_INIT_AUDIO)。
/// 注意：SDL_OpenAudioDevice 不会自动初始化音频子系统，必须先 Init。
namespace
{
    std::once_flag g_sdl_audio_once;     ///< 保证 InitSubSystem 只执行一次
    std::mutex     g_sdl_audio_init_mtx; ///< 保护 g_sdl_audio_ref 增减
    int            g_sdl_audio_ref = 0;  ///< 当前存活的播放器实例数

    /// \brief 增加 SDL 音频子系统引用计数，首次调用时初始化
    /// \return 初始化成功 true，失败 false
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

    /// \brief 减少引用计数，归零时退出 SDL 音频子系统
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

XAudioPlay::~XAudioPlay() = default;

auto XAudioPlay::chunkRemainBytes(const AudioChunk &chunk) -> std::size_t
{
    if (chunk.offset >= chunk.data.size())
    {
        return 0;
    }
    return chunk.data.size() - chunk.offset;
}

auto XAudioPlay::push(const uint8_t *data, int size) -> bool
{
    if (!data || size <= 0)
    {
        return false;
    }

    std::scoped_lock lock(queue_mtx_);

    /// 背压：超过上限则拒绝，避免解码线程过快导致内存暴涨
    if (max_queue_bytes_ > 0)
    {
        const auto current = queuedBytesLocked();
        if (current + static_cast<std::size_t>(size) > max_queue_bytes_)
        {
            return false;
        }
    }

    /// 队尾 emplace_back 新建 AudioChunk，assign 深拷贝 PCM，调用方可立即复用 data 缓冲区
    queue_.emplace_back();
    auto &chunk  = queue_.back();
    chunk.offset = 0;
    chunk.data.assign(data, data + size);
    return true;
}

auto XAudioPlay::clearQueue() -> void
{
    std::scoped_lock lock(queue_mtx_);
    clearQueueLocked();
}

auto XAudioPlay::queuedBytesLocked() const -> std::size_t
{
    std::size_t total = 0;
    for (const auto &chunk : queue_)
    {
        total += chunkRemainBytes(chunk);
    }
    return total;
}

auto XAudioPlay::queuedBytes() const -> std::size_t
{
    std::scoped_lock lock(queue_mtx_);
    return queuedBytesLocked();
}

auto XAudioPlay::setMaxQueueBytes(std::size_t bytes) -> void
{
    std::scoped_lock lock(queue_mtx_);
    max_queue_bytes_ = bytes;
}

auto XAudioPlay::maxQueueBytes() const -> std::size_t
{
    std::scoped_lock lock(queue_mtx_);
    return max_queue_bytes_;
}

auto XAudioPlay::clearQueueLocked() -> void
{
    queue_.clear();
}

auto XAudioPlay::setSpeed(double speed) -> void
{
    if (speed <= 0.0 || speed > 10.0)
    {
        return;
    }
    speed_.store(speed, std::memory_order_relaxed);
}

auto XAudioPlay::getSpeed() const -> double
{
    return speed_.load(std::memory_order_relaxed);
}

auto XAudioPlay::setVolume(double volume) -> void
{
    volume_.store(std::clamp(volume, 0.0, 1.0), std::memory_order_relaxed);
}

auto XAudioPlay::getVolume() const -> double
{
    return volume_.load(std::memory_order_relaxed);
}

auto XAudioPlay::applyVolumeToS16(uint8_t *data, int bytes, double volume) -> void
{
    if (!data || bytes <= 0 || volume >= 1.0)
    {
        return;
    }

    if (volume <= 0.0)
    {
        std::memset(data, 0, static_cast<std::size_t>(bytes));
        return;
    }

    auto       *samples = reinterpret_cast<int16_t *>(data);
    const int   count   = bytes / static_cast<int>(sizeof(int16_t));
    for (int i = 0; i < count; ++i)
    {
        const int scaled = static_cast<int>(std::lround(static_cast<double>(samples[i]) * volume));
        samples[i]       = static_cast<int16_t>(std::clamp(scaled, -32768, 32767));
    }
}

auto XAudioPlay::readByteAtLocked(std::size_t abs_index, uint8_t &out) const -> bool
{
    std::size_t skip = abs_index;
    for (const auto &chunk : queue_)
    {
        const auto remain = chunkRemainBytes(chunk);
        if (skip < remain)
        {
            out = chunk.data[chunk.offset + skip];
            return true;
        }
        skip -= remain;
    }
    return false;
}

auto XAudioPlay::discardBytesLocked(std::size_t count) -> void
{
    while (count > 0 && !queue_.empty())
    {
        auto      &front  = queue_.front();
        const auto remain = chunkRemainBytes(front);
        if (remain <= count)
        {
            count -= remain;
            queue_.pop_front();
        }
        else
        {
            front.offset += count;
            count = 0;
        }
    }
}

auto XAudioPlay::drainPcm(uint8_t *dst, int len) -> void
{
    if (!dst || len <= 0)
    {
        return;
    }

    std::memset(dst, 0, static_cast<std::size_t>(len));

    double speed = speed_.load(std::memory_order_relaxed);
    if (speed <= 0.0)
    {
        speed = 1.0;
    }

    {
        std::scoped_lock lock(queue_mtx_);
        /// speed≈1.0：块拷贝快路径（与 ffmpeg_audio_sdl 回调语义一致）
        if (std::abs(speed - 1.0) < 1e-6)
        {
            int mixed = 0;
            while (mixed < len && !queue_.empty())
            {
                auto      &front  = queue_.front();
                const auto remain = static_cast<int>(front.data.size() - front.offset);
                if (remain <= 0)
                {
                    queue_.pop_front();
                    continue;
                }

                const int need = len - mixed;
                const int copy = std::min(remain, need);
                std::memcpy(dst + mixed, front.data.data() + front.offset, static_cast<std::size_t>(copy));
                mixed += copy;
                front.offset += static_cast<std::size_t>(copy);

                if (front.offset >= front.data.size())
                {
                    queue_.pop_front();
                }
            }
        }
        else
        {
            /// 倍速：按步进索引抽样，音调随倍速变化
            int    out       = 0;
            double src_index = 0.0;
            while (out < len)
            {
                uint8_t byte = 0;
                if (!readByteAtLocked(static_cast<std::size_t>(src_index), byte))
                {
                    break;
                }
                dst[out++] = byte;
                src_index += speed;
            }

            if (out > 0)
            {
                const auto consumed = static_cast<std::size_t>((out - 1) * speed) + 1;
                discardBytesLocked(consumed);
            }
        }
    }

    const double volume = volume_.load(std::memory_order_relaxed);
    applyVolumeToS16(dst, len, volume);
}

/// \brief XAudioPlay 的 SDL2 具体实现
/// SDL 在独立高优先级线程周期性调用 sdlCallback，要求快速返回；
/// 回调内仅调用 drainPcm，禁止文件 IO、解码等耗时操作。
class XSDLAudioPlay final : public XAudioPlay
{
public:
    XSDLAudioPlay() = default;
    ~XSDLAudioPlay() override
    {
        close();
    }

    auto open(const Spec &spec) -> bool override
    {
        close();
        if (!refSdlAudio())
        {
            last_error_ = "SDL audio init failed";
            return false;
        }
        sdl_inited_ = true;

        SDL_AudioSpec want{};
        want.freq     = spec.sample_rate;
        want.format   = AUDIO_S16SYS; ///< 固定 S16 系统字节序
        want.channels = static_cast<Uint8>(spec.channels);
        want.samples  = static_cast<Uint16>(spec.samples);
        want.callback = &XSDLAudioPlay::sdlCallback;
        want.userdata = this;

        /// 仅允许频率/声道微调，禁止 SDL 把格式改成 F32 等导致 S16 PCM 解释错误
        const int allowed_change = SDL_AUDIO_ALLOW_FREQUENCY_CHANGE | SDL_AUDIO_ALLOW_CHANNELS_CHANGE;
        device_id_               = SDL_OpenAudioDevice(nullptr, 0, &want, &obtained_, allowed_change);
        if (device_id_ == 0)
        {
            last_error_ = SDL_GetError();
            LOGE("SDL_OpenAudioDevice failed: " << last_error_);
            close();
            return false;
        }

        if (obtained_.format != AUDIO_S16SYS)
        {
            last_error_ = "SDL device format is not AUDIO_S16SYS";
            LOGE(last_error_ << ", got format=" << static_cast<int>(obtained_.format));
            close();
            return false;
        }

        spec_   = spec;
        opened_ = true;
        LOGI("XAudioPlay opened: " << obtained_.freq << "Hz " << static_cast<int>(obtained_.channels) << "ch"
                                   << " format=S16");
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
        clearQueue();
        setSpeed(1.0);
        setVolume(1.0);
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
    /// \brief SDL 音频设备回调，在音频线程执行
    /// \param[in] userdata 即 this 指针
    /// \param[out] stream 待填充的 PCM 缓冲
    /// \param[in] len 本次需要的字节数
    static void SDLCALL sdlCallback(void *userdata, Uint8 *stream, int len)
    {
        auto *self = static_cast<XSDLAudioPlay *>(userdata);
        if (!self)
        {
            SDL_memset(stream, 0, len);
            return;
        }
        self->drainPcm(stream, len);
    }

    Spec              spec_{};             ///< 用户请求的 open 参数
    SDL_AudioSpec     obtained_{};         ///< 设备实际生效的 AudioSpec
    SDL_AudioDeviceID device_id_  = 0;     ///< SDL 设备 ID，0 表示未打开
    bool              opened_     = false; ///< 是否已成功 open
    bool              paused_     = false; ///< 是否处于暂停状态
    bool              sdl_inited_ = false; ///< 本实例是否已占用 SDL 音频子系统引用
    std::string       last_error_;         ///< 最近一次错误信息
};

/// \brief 根据后端类型创建具体播放器
/// \param[in] type 后端枚举
/// \return 堆对象；未知类型返回 nullptr
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
