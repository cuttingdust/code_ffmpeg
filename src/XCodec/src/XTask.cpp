#include "XTask.h"
#include "AVLog.h"

XTask::~XTask()
{
    LOGD("任务销毁: " << getName());
    stop();
    wait();
    reset();
}

void XTask::stop()
{
    XThread::stop();
    queue_cv_.notify_all();

    if (next_)
    {
        next_->stop();
    }
}

void XTask::addObserver(std::shared_ptr<XTask> observer)
{
    if (!observer)
    {
        LOGW("尝试添加空观察者: " << getName());
        return;
    }

    std::scoped_lock lock(observer_mutex_);

    /// 避免重复添加
    if (std::ranges::find(observers_, observer) != observers_.end())
    {
        LOGW("观察者已存在: " << observer->getName());
        return;
    }

    observers_.push_back(observer);
    LOGI("添加观察者: " << observer->getName() << " -> " << getName());
}

void XTask::removeObserver(std::shared_ptr<XTask> observer)
{
    if (!observer)
    {
        return;
    }

    std::scoped_lock lock(observer_mutex_);

    auto it = std::ranges::find(observers_, observer);
    if (it != observers_.end())
    {
        observers_.erase(it);
        LOGI("移除观察者: " << observer->getName());
    }
}

void XTask::notifyObservers(PacketWrapper::Ptr pkt)
{
    if (!pkt || observers_.empty())
    {
        return;
    }

    /// 复制观察者列表，避免在遍历时被修改
    std::vector<std::shared_ptr<XTask>> observers_copy;
    {
        std::scoped_lock lock(observer_mutex_);
        observers_copy = observers_;
    }

    for (auto& observer : observers_copy)
    {
        if (!observer)
        {
            continue;
        }

        auto clone = pkt->clone();
        try
        {
            if (observer->getQueueSize() < observer->getMaxQueueSize())
            {
                observer->pushPacket(std::move(clone));
            }
            else
            {
                LOGW("观察者队列已满，丢弃包: " << observer->getName());
            }
        }
        catch (const std::exception& e)
        {
            LOGE("克隆数据包失败，观察者: " << observer->getName() << ", 错误: " << e.what());
        }
    }
}

void XTask::reset()
{
    LOGD("重置任务: " << getName());

    {
        std::scoped_lock lock(queue_mutex_);
        while (!packet_queue_.empty())
        {
            packet_queue_.pop();
        }
        while (!frame_queue_.empty())
        {
            av_frame_free(&frame_queue_.front());
            frame_queue_.pop();
        }
    }

    eof_reached_ = false;
}

void XTask::clear()
{
    LOGD("清空任务队列: " << getName());

    {
        std::scoped_lock lock(queue_mutex_);
        while (!packet_queue_.empty())
        {
            packet_queue_.pop();
        }
        while (!frame_queue_.empty())
        {
            av_frame_free(&frame_queue_.front());
            frame_queue_.pop();
        }
    }
    // 注意：不清空 eof_reached_
}

void XTask::notifyEof()
{
    eof_reached_ = true;
    queue_cv_.notify_all();

    if (next_)
    {
        next_->notifyEof();
    }
}

auto XTask::pushPacket(PacketWrapper::Ptr pkt) -> void
{
    std::unique_lock<std::mutex> lock(queue_mutex_);
    queue_cv_.wait(lock, [this]() { return packet_queue_.size() < max_queue_size_ || shouldStop(); });
    packet_queue_.push(std::move(pkt));
    queue_cv_.notify_one();
}

auto XTask::pushFrame(AVFrame* frame) -> void
{
    std::unique_lock<std::mutex> lock(queue_mutex_);
    queue_cv_.wait(lock, [this]() { return frame_queue_.size() < max_queue_size_ || shouldStop(); });
    frame_queue_.push(frame);
    queue_cv_.notify_one();
}

size_t XTask::getQueueSize() const
{
    std::scoped_lock lock(queue_mutex_);
    return packet_queue_.size() + frame_queue_.size();
}

auto XTask::popPacket() -> PacketWrapper::Ptr
{
    std::unique_lock<std::mutex> lock(queue_mutex_);
    auto predicate = [this]() { return !packet_queue_.empty() || eof_reached_ || shouldStop(); };

    if (!queue_cv_.wait_for(lock, std::chrono::milliseconds(100), predicate))
    {
        return nullptr;
    }

    if (packet_queue_.empty() || shouldStop())
    {
        return nullptr;
    }

    auto pkt = std::move(packet_queue_.front());
    packet_queue_.pop();
    return pkt;
}

auto XTask::popFrame() -> AVFrame*
{
    std::unique_lock<std::mutex> lock(queue_mutex_);
    auto                         predicate = [this]() { return !frame_queue_.empty() || eof_reached_ || shouldStop(); };

    if (!queue_cv_.wait_for(lock, std::chrono::milliseconds(100), predicate))
    {
        return nullptr;
    }

    if (frame_queue_.empty() || shouldStop())
    {
        return nullptr;
    }

    auto frame = frame_queue_.front();
    frame_queue_.pop();
    return frame;
}

auto XTask::handleError(const std::string& msg) -> void
{
    LOGE("任务错误 [" << getName() << "]: " << msg);
    if (error_cb_)
    {
        error_cb_(msg);
    }
}

void XTask::run()
{
    LOGI("任务线程启动: " << getName());

    try
    {
        process();
    }
    catch (const std::exception& e)
    {
        handleError(std::string("异常: ") + e.what());
    }
    catch (...)
    {
        handleError("未知异常");
    }

    LOGI("任务线程结束: " << getName());
}
