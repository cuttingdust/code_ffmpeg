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

void XTask::handleError(const std::string& msg)
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
