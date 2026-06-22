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
    std::mutex g_sdl_audio_init_mtx;
    int        g_sdl_audio_ref = 0;

    auto refSdlAudio() -> bool
    {
        std::scoped_lock lock(g_sdl_audio_init_mtx);
        if (g_sdl_audio_ref == 0)
        {
            if ((SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO) == 0)
            {
                if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0)
                {
                    LOGE("SDL_InitSubSystem(AUDIO) failed: " << SDL_GetError());
                    return false;
                }
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
        // std::memset(data, 0, static_cast<std::size_t>(bytes));
        SDL_memset(data, 0, static_cast<std::size_t>(bytes));
        return;
    }

    auto     *samples = reinterpret_cast<int16_t *>(data);
    const int count   = bytes / static_cast<int>(sizeof(int16_t));
    for (int i = 0; i < count; ++i)
    {
        const int scaled = static_cast<int>(std::lround(static_cast<double>(samples[i]) * volume));
        samples[i]       = static_cast<int16_t>(std::clamp(scaled, -32768, 32767));
    }
}

/// \brief 从队列「逻辑字节流」指定位置读一字节（跨多块 AudioChunk 寻址）
/// \param[in] abs_index 相对队头第 0 字节的绝对偏移（0 = 队头 offset 处第一个未消费字节）
/// \param[out] out 读到的字节
/// \return 成功 true；abs_index 超出队列剩余总字节时 false
/// \note 调用方须已持有 queue_mtx_；drainPcmWithSpeed 经 readFrameAtLocked 间接调用
auto XAudioPlay::readByteAtLocked(std::size_t abs_index, uint8_t &out) const -> bool
{
    /// skip：当前要在「逻辑流」上前进多少字节，从队头块起逐块扣减
    /// 例：块A剩500、块B剩3000，abs_index=600 → 跳过块A(500)，在块B的 offset+100 处读
    std::size_t skip = abs_index;
    for (const auto &chunk : queue_)
    {
        const auto remain = chunkRemainBytes(chunk); ///< 该块未消费字节 = data.size() - offset
        if (skip < remain)
        {
            /// 目标落在此块内：chunk.data[offset + skip]
            out = chunk.data[chunk.offset + skip];
            return true;
        }
        /// 目标在此块之后：整块跳过，继续在下一 chunk 上找
        skip -= remain;
    }
    /// 所有块都跳完仍不够 abs_index，队列数据不足
    return false;
}

/// \brief 从队头逻辑流丢弃 count 字节（推进 offset / pop_front，与 drainPcm 快路径消费语义一致）
/// \param[in] count 要丢弃的字节数；为 0 时无操作
/// \note 调用方须已持有 queue_mtx_；drainPcmWithSpeed 在生成输出后按整帧字节数调用
auto XAudioPlay::discardBytesLocked(std::size_t count) -> void
{
    while (count > 0 && !queue_.empty())
    {
        auto      &front  = queue_.front(); ///< 始终从队头块开始丢
        const auto remain = chunkRemainBytes(front);
        if (remain <= count)
        {
            /// 队头块不够或刚好够：整块弹出，count 减去已丢字节，继续处理下一块
            count -= remain;
            queue_.pop_front();
        }
        else
        {
            /// 队头块足够：只推进 offset，不 pop；与 drainPcm 中 front.offset += copy 相同
            front.offset += count;
            count = 0;
        }
    }
}

auto XAudioPlay::setPlaybackFormat(int channels) -> void
{
    if (channels <= 0)
    {
        channels = 2;
    }
    playback_channels_    = channels;
    playback_frame_bytes_ = channels * static_cast<int>(sizeof(int16_t));
}

/// \brief 从队列逻辑帧位置读取一帧 S16 交错 PCM
/// \param[in] frame_index 相对队头的帧号（0=队头起第一帧，1=第二帧…）
/// \param[out] dst_frame 输出缓冲，至少 playback_channels_ 个 int16_t
/// \return 该帧任一声道字节越界（队列不够）时 false
/// \note 调用方须已持有 queue_mtx_；内部把帧号换算为字节偏移后逐声道 readByteAtLocked
/// \note 立体声一帧布局：[L0][R0] 共 frame_bytes 字节；frame_index=2 从字节 2*frame_bytes 起读
auto XAudioPlay::readFrameAtLocked(std::size_t frame_index, int16_t *dst_frame) const -> bool
{
    if (!dst_frame || playback_frame_bytes_ <= 0)
    {
        return false;
    }

    /// base_byte：第 frame_index 帧在逻辑字节流中的起始偏移
    /// 例 channels=2、frame_bytes=4：frame_index=3 → base_byte=12（第 4 帧从第 12 字节开始）
    const auto base_byte = frame_index * static_cast<std::size_t>(playback_frame_bytes_);
    for (int ch = 0; ch < playback_channels_; ++ch)
    {
        /// 交错格式：声道 ch 的 int16 起始于 base_byte + ch*2
        const auto byte_index = base_byte + static_cast<std::size_t>(ch) * sizeof(int16_t);
        uint8_t    lo         = 0; ///< S16 低字节
        uint8_t    hi         = 0; ///< S16 高字节
        if (!readByteAtLocked(byte_index, lo) || !readByteAtLocked(byte_index + 1, hi))
        {
            /// 任一字节读失败说明队列不够这一帧，调用方（drainPcmWithSpeed）应 break
            return false;
        }
        /// 按本机字节序拼成 int16_t 写入 dst_frame[ch]（与 AUDIO_S16SYS 一致）
        uint8_t bytes[2] = { lo, hi };
        SDL_memcpy(&dst_frame[ch], bytes, sizeof(int16_t));
    }
    return true;
}

/// \brief 倍速路径：按采样帧线性插值从队列生成输出 PCM 并丢弃已消费帧
/// \param[out] dst 输出缓冲（与 drainPcm 相同，已由 SDL_memset 预填静音）
/// \param[in] len 本次 SDL 回调需要的输出字节数
/// \param[in] speed 当前倍速（由 drainPcm 传入，已保证 >0）；>1 快放变调升高，<1 慢放变调降低
/// \note 调用方须已持有 queue_mtx_；变速变调，非 atempo 保音调方案
/// \note 必须以「帧」为单位读写，禁止按字节跳读，否则会破坏 S16 立体声交错对齐
auto XAudioPlay::drainPcmWithSpeed(uint8_t *dst, int len, double speed) -> void
{
    const int frame_bytes = playback_frame_bytes_; ///< 单帧字节 = channels * sizeof(int16_t)，立体声常为 4
    const int channels    = playback_channels_;    ///< 声道数，插值时逐声道独立计算
    if (frame_bytes <= 0 || channels <= 0 || len < frame_bytes)
    {
        return;
    }

    /// out_frames：dst 能容纳的完整帧数（尾部不足一帧的字节不处理，drainPcm 已静音）
    const int out_frames = len / frame_bytes;
    /// produced：已成功写入 dst 的输出帧计数；队列不够 readFrameAtLocked 时 break，produced < out_frames
    int produced = 0;

    /// 对 dst 的每一输出帧 i，在源队列上取「逻辑帧号」src_frame = i * speed 并插值
    /// 例 speed=1.1：i=0→源0.0，i=1→源1.1（在源帧1与2之间插值），i=2→源2.2 …
    for (int i = 0; i < out_frames; ++i)
    {
        /// src_frame：第 i 个输出帧对应源 PCM 的浮点帧索引（相对队头第 0 帧）
        const double src_frame = static_cast<double>(i) * speed;
        /// f0：src_frame 的整数部分，插值左端点帧号；frac ∈ [0,1) 为两帧之间的权重
        const auto   f0   = static_cast<std::size_t>(src_frame);
        const double frac = src_frame - static_cast<double>(f0);

        int16_t frame0[8] = {}; ///< 源帧 f0 各声道采样（最多 8 声道，实际用 channels）
        int16_t frame1[8] = {}; ///< 源帧 f0+1，仅 frac>0 时需要
        if (!readFrameAtLocked(f0, frame0))
        {
            /// 队头往后数不到 f0 帧，无法继续生成，dst[i..] 保持静音
            break;
        }

        /// frac≈0 时落在整帧上，只用 frame0；否则 frame1 与 frame0 线性插值
        const bool has_f1 = frac > 1e-9 && readFrameAtLocked(f0 + 1, frame1);
        int16_t   *out    = reinterpret_cast<int16_t *>(dst + static_cast<std::size_t>(i) * frame_bytes);
        for (int ch = 0; ch < channels; ++ch)
        {
            const double s0 = static_cast<double>(frame0[ch]);                      ///< 左端点样本
            const double s1 = has_f1 ? static_cast<double>(frame1[ch]) : s0;        ///< 右端点；无 f1 则退化为 s0
            const int    v  = static_cast<int>(std::lround(s0 + (s1 - s0) * frac)); ///< 插值并四舍五入
            out[ch]         = static_cast<int16_t>(std::clamp(v, -32768, 32767));
        }
        ++produced;
    }

    if (produced > 0)
    {
        /// 根据「最后写出的输出帧」反推队头应丢弃多少源帧（整帧对齐 discard）
        /// end_src：最后一帧输出（下标 produced-1）在源侧的浮点帧号
        const double end_src = static_cast<double>(produced - 1) * speed;
        /// last_frame：end_src 的整数部分；若小数部分非 0，插值还读过 f0+1 帧
        const auto last_frame  = static_cast<std::size_t>(end_src);
        const bool need_interp = (end_src - static_cast<double>(last_frame)) > 1e-9;
        /// consumed_frames：队头起已「用到」的源帧数 = [0..last_frame] 共 last_frame+1 帧，插值再加 1 帧
        const auto consumed_frames = last_frame + 1 + (need_interp ? 1U : 0U);
        discardBytesLocked(consumed_frames * static_cast<std::size_t>(frame_bytes));
    }
}

/// \brief 从队列取出 PCM 填入声卡回调缓冲区（SDL 音频线程入口）
/// \param[out] dst 即 sdlCallback 的 stream；由 SDL 提供，本函数写入 S16 PCM 后交声卡播放
/// \param[in] len 本次回调需要的字节数，通常 = samples * channels * 2（如 1024*2*2=4096）
/// \note 流程：静音预填 → 持 queue_mtx_ 从 queue_ 消费 → applyVolumeToS16
/// \note speed≈1.0 走 SDL_memcpy 快路径；否则 drainPcmWithSpeed 按帧插值
/// \note 队列不足 len 时，dst 未覆盖区间保持静音（underrun）
auto XAudioPlay::drainPcm(uint8_t *dst, int len) -> void
{
    if (!dst || len <= 0)
    {
        return;
    }

    /// 先整段置零：队列空或数据不够时，声卡听到的是静音而不是脏内存
    SDL_memset(dst, 0, static_cast<std::size_t>(len));

    /// 主线程 setSpeed 写入；relaxed 即可，晚一两帧切速一般可接受
    double speed = speed_.load(std::memory_order_relaxed);
    if (speed <= 0.0)
    {
        speed = 1.0;
    }

    {
        /// push() 与 drainPcm() 可能并发，必须同一把锁保护 queue_
        std::scoped_lock lock(queue_mtx_);
        if (std::abs(speed - 1.0) < 1e-6)
        {
            /// --- 1.0 倍速快路径：按字节块拷贝，不做重采样 ---
            /// 变量关系（每轮循环）：
            ///   len    — 入参，本次 SDL 回调要填满的 dst 总字节数（固定，如 4096）
            ///   mixed  — dst 已写入字节数，[0, len)；循环直到 mixed == len 或队列空
            ///   front  — queue_.front()，队头 AudioChunk（data=PCM 副本，offset=已消费偏移）
            ///   remain — 队头块未读字节 = front.data.size() - front.offset
            ///   need   — dst 剩余可写 = len - mixed
            ///   copy   — 本轮实际搬运 = min(remain, need)
            /// 拷贝：front.data[offset .. offset+copy) → dst[mixed .. mixed+copy)
            int mixed = 0;                         ///< dst 写入进度；dst[0..mixed) 已有 PCM，dst[mixed..len) 待填
            while (mixed < len && !queue_.empty()) ///< mixed<len：dst 未满；非空：队列还有 PCM
            {
                auto &front = queue_.front(); ///< 当前从哪一块读；一块可能跨多次回调才读完
                /// remain：这块里还剩多少字节没播；offset 是上次回调留下的续读位置
                const auto remain = static_cast<int>(front.data.size() - front.offset);
                if (remain <= 0)
                {
                    /// 块已读空（offset 已到 data.size()），丢弃后看下一块
                    queue_.pop_front();
                    continue;
                }

                const int need = len - mixed;            ///< dst 还差多少字节才满
                const int copy = std::min(remain, need); ///< 受队头剩余与 dst 剩余双重限制
                SDL_memcpy(dst + mixed, front.data.data() + front.offset, static_cast<std::size_t>(copy));
                mixed += copy;                                  ///< dst 进度前进 copy 字节
                front.offset += static_cast<std::size_t>(copy); ///< 队头读指针前进，下次回调从此续读

                if (front.offset >= front.data.size())
                {
                    /// 本轮拷完后整块用完，弹出；下一块（若有）成为新 front
                    queue_.pop_front();
                }
            }
            /// 退出时 mixed==len（填满）或 queue_ 空（underrun：dst[mixed..len) 保持静音）
        }
        else
        {
            /// --- 倍速路径：按采样帧线性插值，音调随 speed 变化（非 atempo 保音调）---
            drainPcmWithSpeed(dst, len, speed);
        }
    }

    /// 音量缩放放在锁外：只读写 dst 与 volume_，不访问 queue_
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
        constexpr int allowed_change = SDL_AUDIO_ALLOW_FREQUENCY_CHANGE | SDL_AUDIO_ALLOW_CHANNELS_CHANGE;
        device_id_                   = SDL_OpenAudioDevice(nullptr, 0, &want, &obtained_, allowed_change);
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

        spec_ = spec;
        setPlaybackFormat(static_cast<int>(obtained_.channels));
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
