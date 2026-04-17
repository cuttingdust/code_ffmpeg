#pragma once

#include "XThread.h"
#include "PacketWrapper.h"
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <memory>
#include <functional>

/// 基础任务类（继承自XThread）
class XTask : public XThread
{
public:
    XTask() = default;
    ~XTask() override;

    /// 设置下一个任务
    void setNext(std::shared_ptr<XTask> next)
    {
        next_ = next;
    }

    /// 获取下一个任务
    auto getNext() const -> std::shared_ptr<XTask>
    {
        return next_;
    }

    /// 推送数据包到任务队列
    auto pushPacket(PacketWrapper::Ptr pkt) -> void;

    /// 推送帧到任务队列（原始指针）
    auto pushFrame(AVFrame* frame) -> void;

    /// 获取队列大小
    size_t getQueueSize() const;

    /// 设置最大队列大小
    void setMaxQueueSize(size_t size)
    {
        max_queue_size_ = size;
    }

    /// 获取最大队列大小
    size_t getMaxQueueSize() const
    {
        return max_queue_size_;
    }

    /// 设置空闲超时时间（毫秒）
    void setIdleTimeoutMs(int ms)
    {
        idle_timeout_ms_ = ms;
    }

    /// 获取空闲超时时间
    int getIdleTimeoutMs() const
    {
        return idle_timeout_ms_;
    }

    /// 设置错误回调
    void setErrorCallback(const std::function<void(const std::string&)>& cb)
    {
        error_cb_ = cb;
    }

    /// 获取EOF状态
    bool isEofReached() const
    {
        return eof_reached_;
    }

    /// 通知结束（由上游调用）
    void notifyEof();

    /// 重置任务（清空队列 + 重置状态）
    virtual void reset();

    /// 只清空队列，不重置状态
    virtual void clear();

    /// 重写stop方法，唤醒等待
    void stop() override;

    virtual void flushDownstream();

    // ==================== 观察者模式支持 ====================

    /// 添加观察者
    void addObserver(std::shared_ptr<XTask> observer);

    /// 移除观察者
    void removeObserver(std::shared_ptr<XTask> observer);

    /// 通知所有观察者（数据包）
    void notifyObservers(PacketWrapper::Ptr pkt);

protected:
    /// 从队列获取数据包（阻塞，带超时）
    auto popPacket() -> PacketWrapper::Ptr;

    /// 从队列获取帧（阻塞，带超时）
    auto popFrame() -> AVFrame*;

    /// 处理错误
    auto handleError(const std::string& msg) -> void;

    /// 纯虚函数：任务处理逻辑
    virtual auto process() -> void = 0;


    /// 线程主函数（final禁止子类重写）
    auto run() -> void final;

protected:
    std::shared_ptr<XTask> next_;

    /// 双队列：包队列用智能指针，帧队列用原始指针
    std::queue<PacketWrapper::Ptr> packet_queue_;
    std::queue<AVFrame*>           frame_queue_;
    mutable std::mutex             queue_mutex_;
    std::condition_variable        queue_cv_;

    size_t            max_queue_size_ = 100;
    std::atomic<bool> eof_reached_{ false };
    int               idle_timeout_ms_ = 3000; // 默认3秒

    // 错误回调
    std::function<void(const std::string&)> error_cb_;

    /// ==================== 观察者相关成员 ====================
    std::vector<std::shared_ptr<XTask>> observers_;
    mutable std::mutex                  observer_mutex_;
};
