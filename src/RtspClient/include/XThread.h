#pragma once

#include "AVConst.h"

#include <thread>
#include <atomic>


class XThread
{
public:
    XThread();
    virtual ~XThread();

    /// 禁止拷贝
    XThread(const XThread &)            = delete;
    XThread &operator=(const XThread &) = delete;

public:
    /// 启动线程
    virtual auto start() -> void;

    /// 停止线程
    virtual auto stop() -> void;

    /// 等待线程结束
    auto wait() -> void;

    /// 检查是否正在运行
    auto isRunning() const -> bool;

    /// 获取线程ID
    auto getThreadId() const -> std::thread::id;

    /// 设置线程名称
    auto setName(const std::string &name) -> void;

    /// 获取线程名称
    auto getName() const -> std::string;

protected:
    /// 线程主函数，由子类实现
    virtual auto run() -> void = 0;

    /// 检查是否需要停止
    auto shouldStop() const -> bool;

    /// 睡眠指定毫秒
    auto sleep(int ms) -> void;

private:
    class PImpl;
    std::unique_ptr<PImpl> impl_;
    std::string            thread_name_;
    std::atomic<bool>      is_running_{ false };
};
