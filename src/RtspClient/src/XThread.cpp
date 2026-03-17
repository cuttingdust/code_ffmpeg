#include "XThread.h"
#include "AVLog.h"

class XThread::PImpl
{
public:
    PImpl(XThread* owner);
    ~PImpl() = default;

public:
    auto exec() -> void;

public:
    XThread*        owner_ = nullptr;
    std::thread     thread_;
    std::thread::id thread_id_;
};

XThread::PImpl::PImpl(XThread* owner) : owner_(owner)
{
}


auto XThread::PImpl::exec() -> void
{
    thread_id_ = std::this_thread::get_id();
    LOGD("线程 [" << owner_->getName() << "] 启动，ID: " << thread_id_);

    try
    {
        owner_->run();
    }
    catch (const std::exception& e)
    {
        LOGE("线程 [" << owner_->getName() << "] 异常: " << e.what());
    }
    catch (...)
    {
        LOGE("线程 [" << owner_->getName() << "] 未知异常");
    }

    LOGD("线程 [" << owner_->getName() << "] 结束");
}

XThread::XThread()
{
    impl_ = std::make_unique<PImpl>(this);
    LOGD("XThread 创建");
}

XThread::~XThread()
{
    LOGD("XThread 销毁");
    XThread::stop();
    wait();
}

auto XThread::start() -> void
{
    if (is_running_)
    {
        return;
    }


    is_running_    = true;
    impl_->thread_ = std::thread([this]() { impl_->exec(); });
    LOGD("线程 [" << thread_name_ << "] 开始运行");
}

auto XThread::stop() -> void
{
    if (!is_running_)
    {
        return;
    }


    LOGD("线程 [" << thread_name_ << "] 停止中...");
    is_running_ = false;
}

auto XThread::wait() -> void
{
    if (impl_->thread_.joinable())
    {
        impl_->thread_.join();
        LOGD("线程 [" << thread_name_ << "] 已等待结束");
    }
}

auto XThread::isRunning() const -> bool
{
    return is_running_;
}

auto XThread::getThreadId() const -> std::thread::id
{
    return impl_->thread_id_;
}

auto XThread::setName(const std::string& name) -> void
{
    thread_name_ = name;
}

auto XThread::getName() const -> std::string
{
    return thread_name_;
}

auto XThread::shouldStop() const -> bool
{
    return !is_running_;
}

auto XThread::sleep(int ms) -> void
{
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}
