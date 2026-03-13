#include "XTask.h"
#include "AVLog.h"

XTask::~XTask()
{
    LOGD("任务销毁: " << getName());
    stop();
    wait();

    // 清理队列
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
}

void XTask::notifyEof()
{
    eof_reached_ = true;
    queue_cv_.notify_all();

    // 传递给下一个任务
    if (next_)
    {
        next_->notifyEof();
    }
}

void XTask::pushPacket(std::unique_ptr<PacketWrapper> pkt)
{
    std::unique_lock<std::mutex> lock(queue_mutex_);

    // 背压控制：如果队列太大，等待
    queue_cv_.wait(lock, [this]() { return packet_queue_.size() < max_queue_size_ || shouldStop(); });

    packet_queue_.push(std::move(pkt));
    queue_cv_.notify_one();
}

void XTask::pushFrame(AVFrame* frame)
{
    std::unique_lock<std::mutex> lock(queue_mutex_);

    queue_cv_.wait(lock, [this]() { return frame_queue_.size() < max_queue_size_ || shouldStop(); });

    frame_queue_.push(frame);
    queue_cv_.notify_one();
}

size_t XTask::getQueueSize() const
{
    std::lock_guard<std::mutex> lock(queue_mutex_);
    return packet_queue_.size() + frame_queue_.size();
}

std::unique_ptr<PacketWrapper> XTask::popPacket()
{
    std::unique_lock<std::mutex> lock(queue_mutex_);

    auto predicate = [this]() { return !packet_queue_.empty() || eof_reached_ || shouldStop(); };

    queue_cv_.wait(lock, predicate);

    if (packet_queue_.empty() || shouldStop())
    {
        return nullptr;
    }

    auto pkt = std::move(packet_queue_.front());
    packet_queue_.pop();
    return pkt;
}

AVFrame* XTask::popFrame()
{
    std::unique_lock<std::mutex> lock(queue_mutex_);

    auto predicate = [this]() { return !frame_queue_.empty() || eof_reached_ || shouldStop(); };

    queue_cv_.wait(lock, predicate);

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
