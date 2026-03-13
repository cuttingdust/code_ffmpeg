#pragma once

#include "AVConst.h"

/// AVDictionary的RAII包装
class DictWrapper
{
public:
    DictWrapper();
    ~DictWrapper();

    // 移动构造函数
    DictWrapper(DictWrapper&& other) noexcept;

    // 移动赋值操作符
    DictWrapper& operator=(DictWrapper&& other) noexcept;

    // 禁止拷贝
    DictWrapper(const DictWrapper&)            = delete;
    DictWrapper& operator=(const DictWrapper&) = delete;

public:
    auto get_ptr() -> AVDictionary**;

    auto get() const -> AVDictionary*;

    auto set(const std::string& key, const std::string& value) -> void;

    auto print(const std::string& title) const -> void;

    auto check_unused() const -> void;

    /// 清空字典
    auto clear() -> void;

private:
    class PImpl;
    std::unique_ptr<PImpl> impl_;
};
