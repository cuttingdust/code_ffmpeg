#include "XThread.h"
#include "AVLog.h"

class XThread::PImpl
{
public:
    PImpl(XThread* owner) : owner_(owner)
    {
    }
    ~PImpl() = default;

    void exec()
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
        owner_->thread_started_ = false;
        owner_->is_running_     = false;
    }

    XThread*        owner_ = nullptr;
    std::thread     thread_;
    std::thread::id thread_id_;
};

XThread::XThread()
{
    impl_ = std::make_unique<PImpl>(this);
    LOGD("XThread 创建");
}

XThread::~XThread()
{
    LOGD("XThread 销毁");
    stop();
    wait();
}

auto XThread::start() -> void
{
    if (thread_started_)
    {
        LOGW("线程 [" << thread_name_ << "] 已经在运行，忽略重复启动");
        return;
    }

    if (is_running_)
    {
        LOGW("线程 [" << thread_name_ << "] 正在运行，忽略重复启动");
        return;
    }

    is_running_     = true;
    thread_started_ = true;

    try
    {
        impl_->thread_ = std::thread([this]() { impl_->exec(); });
        LOGD("线程 [" << thread_name_ << "] 开始运行");
    }
    catch (const std::system_error& e)
    {
        LOGE("线程 [" << thread_name_ << "] 启动系统错误: " << e.what());
        is_running_     = false;
        thread_started_ = false;
    }
    catch (const std::exception& e)
    {
        LOGE("线程 [" << thread_name_ << "] 启动异常: " << e.what());
        is_running_     = false;
        thread_started_ = false;
    }
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
        try
        {
            impl_->thread_.join();
            LOGD("线程 [" << thread_name_ << "] 已等待结束");
        }
        catch (const std::system_error& e)
        {
            LOGE("线程 [" << thread_name_ << "] join 系统错误: " << e.what());
        }
        catch (const std::exception& e)
        {
            LOGE("线程 [" << thread_name_ << "] join 异常: " << e.what());
        }
    }
    thread_started_ = false;
}

auto XThread::isRunning() const -> bool
{
    return is_running_;
}

auto XThread::getThreadId() const -> std::thread::id
{
    return impl_->thread_id_;
}
