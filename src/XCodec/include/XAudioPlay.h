#pragma once

#include "XCodec_Global.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <vector>

class XCODEC_EXPORT XAudioPlay
{
public:
    enum class Backend
    {
        SDL = 0 ///<<< SDL2 音频设备后端
    };

    /// \brief 打开音频设备时传入的参数（对应 SDL_AudioSpec 子集）
    struct Spec
    {
        int sample_rate = 44100; ///< 采样率，单位 Hz，常见 44100 / 48000
        int channels    = 2;     ///< 声道数，1 单声道，2 立体声
        int samples     = 1024;  ///< 设备单次回调请求的采样帧数，建议 2 的幂，影响延迟
    };

    virtual ~XAudioPlay();

    /// \brief 创建播放器实例（工厂方法，隐藏具体后端类名）
    /// \param[in] type 后端类型，默认 SDL
    /// \return 堆上对象指针，由调用方 delete 或 unique_ptr 管理；不支持时返回 nullptr
    static auto create(Backend type = Backend::SDL) -> XAudioPlay *;

    /// \brief 按 spec 打开音频输出设备
    /// \param[in] spec 采样率、声道数、缓冲帧数
    /// \return 成功返回 true；失败返回 false，原因见 lastError()
    /// \note 若已 open，会先 close 再重新打开；open 后默认设备处于暂停，需 start() 才开始出声
    virtual auto open(const Spec &spec) -> bool = 0;

    /// \brief 开始播放，解除设备暂停（SDL_PauseAudioDevice(dev, 0)）
    virtual auto start() -> void = 0;

    /// \brief 暂停播放（SDL_PauseAudioDevice(dev, 1)），队列数据保留
    virtual auto pause() -> void = 0;

    /// \brief 关闭音频设备、清空缓冲队列并释放 SDL 音频子系统引用
    virtual auto close() -> void = 0;

    /// \brief 设备是否已成功 open
    /// \return 已 open 为 true，否则 false
    virtual auto isOpen() const -> bool = 0;

    /// \brief 获取最近一次 open 等操作失败时的错误描述
    /// \return 错误字符串，无错误时可能为空
    virtual auto lastError() const -> std::string = 0;

    /// \brief 生产者写入一块 PCM 数据（线程安全）
    /// \param[in] data PCM 数据首地址，生命周期仅在本次调用内有效
    /// \param[in] size 字节数，须与 S16 交错格式一致（字节数 = 样本数 * 声道数 * 2）
    /// \return 写入成功 true；data 为空、size<=0 或超过 max_queue_bytes_ 时 false
    /// \note 内部深拷贝到队列尾部新 AudioChunk，不保存调用方指针
    auto push(const uint8_t *data, int size) -> bool;

    /// \brief 清空尚未被 drainPcm 消费的缓冲数据（如 seek、切流、停止时调用）
    auto clearQueue() -> void;

    /// \brief 查询队列中尚未播放的字节总数
    /// \return 所有 AudioChunk 剩余字节之和
    auto queuedBytes() const -> std::size_t;

    /// \brief 设置队列字节上限，用于背压控制
    /// \param[in] bytes 上限字节数；0 表示不限制
    auto setMaxQueueBytes(std::size_t bytes) -> void;

    /// \brief 获取当前队列字节上限
    /// \return 上限值，0 为不限制
    auto maxQueueBytes() const -> std::size_t;

    /// \brief 设置播放倍速（与 XDemuxTask / LocalPlayer 倍速语义一致）
    /// \param[in] speed 倍速，范围 (0, 10]；1.0 为正常，2.0 表示两倍速消耗队列 PCM
    /// \note 通过 drainPcm 按倍速步进读取队列实现，音调会随倍速变化；后期与视频 setSpeed 同步调用
    auto setSpeed(double speed) -> void;

    /// \brief 获取当前播放倍速
    /// \return 倍速值，默认 1.0
    auto getSpeed() const -> double;

    /// \brief 设置输出音量
    /// \param[in] volume 线性音量，范围 [0.0, 1.0]；0.0 静音，1.0 原音量
    /// \note 在 drainPcm 写出声卡缓冲前对 S16 PCM 乘增益，属于播放输出层职责
    auto setVolume(double volume) -> void;

    /// \brief 获取当前输出音量
    /// \return 线性音量，默认 1.0
    auto getVolume() const -> double;

protected:
    /// \brief 从队列取出 PCM 填入声卡回调缓冲区（由子类在音频线程调用）
    /// \param[out] dst SDL 回调提供的输出缓冲，先 memset 为静音再拷贝队列数据
    /// \param[in] len 本次回调需要的字节数，随设备与 samples 变化
    /// \note 队列空时 dst 保持静音；一块数据可跨多次回调通过 offset 分批消费
    auto drainPcm(uint8_t *dst, int len) -> void;

private:
    /// \brief 队列中的单块 PCM 缓冲
    struct AudioChunk
    {
        std::vector<uint8_t> data;       ///< push 时深拷贝的 PCM 副本
        std::size_t          offset = 0; ///< 已被 drainPcm 消费的字节偏移
    };

    /// \brief 计算单个 AudioChunk 中尚未播放的剩余字节
    /// \param[in] chunk 队列中的块
    /// \return 剩余字节数，已全部消费则为 0
    static auto chunkRemainBytes(const AudioChunk &chunk) -> std::size_t;

    /// \brief 清空 queue_（调用方须已持有 queue_mtx_）
    auto clearQueueLocked() -> void;

    /// \brief 统计队列剩余字节（调用方须已持有 queue_mtx_）
    /// \return 未消费字节总数
    auto queuedBytesLocked() const -> std::size_t;

    /// \brief 从队列逻辑位置读取一字节（调用方须已持有 queue_mtx_）
    /// \param[in] abs_index 相对队头的字节偏移
    /// \param[out] out 读到的字节
    /// \return 越界或队列空时 false
    auto readByteAtLocked(std::size_t abs_index, uint8_t &out) const -> bool;

    /// \brief 从队头丢弃若干字节（调用方须已持有 queue_mtx_）
    /// \param[in] count 丢弃字节数
    auto discardBytesLocked(std::size_t count) -> void;

    /// \brief 对 S16 交错 PCM 施加线性音量增益
    /// \param[in,out] data PCM 缓冲
    /// \param[in] bytes 字节数，须为 2 的倍数
    /// \param[in] volume 线性增益 [0.0, 1.0]
    static auto applyVolumeToS16(uint8_t *data, int bytes, double volume) -> void;

    std::deque<AudioChunk> queue_;               ///< PCM 块队列，队头消费队尾生产
    mutable std::mutex     queue_mtx_;           ///< 保护 queue_ 与 max_queue_bytes_
    std::size_t            max_queue_bytes_ = 0; ///< 队列字节上限，0 表示不限
    std::atomic<double>    speed_{ 1.0 };        ///< 播放倍速，drainPcm 按此步进消费队列
    std::atomic<double>    volume_{ 1.0 };       ///< 输出音量，drainPcm 写出前乘增益
};
