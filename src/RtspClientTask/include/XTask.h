#pragma once

#include "XThread.h"
#include "PacketWrapper.h"
#include "FrameWrapper.h"
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
    std::shared_ptr<XTask> getNext() const
    {
        return next_;
    }

    /// 推送数据包到任务队列
    void pushPacket(std::unique_ptr<PacketWrapper> pkt);

    /// 推送帧到任务队列
    void pushFrame(AVFrame* frame);

    /// 获取队列大小
    size_t getQueueSize() const;

    /// 设置最大队列大小
    void setMaxQueueSize(size_t size)
    {
        max_queue_size_ = size;
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

protected:
    /// 从队列获取数据包（阻塞）
    std::unique_ptr<PacketWrapper> popPacket();

    /// 从队列获取帧（阻塞）
    AVFrame* popFrame();

    /// 处理错误
    void handleError(const std::string& msg);

    /// 纯虚函数：任务处理逻辑
    virtual void process() = 0;

    /// 线程主函数（final禁止子类重写）
    void run() final;

protected:
    std::shared_ptr<XTask> next_;

    // 双队列：支持包和帧
    std::queue<std::unique_ptr<PacketWrapper>> packet_queue_;
    std::queue<AVFrame*>                       frame_queue_;
    mutable std::mutex                         queue_mutex_;
    std::condition_variable                    queue_cv_;

    size_t            max_queue_size_ = 100;
    std::atomic<bool> eof_reached_{ false };

    // 错误回调
    std::function<void(const std::string&)> error_cb_;
};
