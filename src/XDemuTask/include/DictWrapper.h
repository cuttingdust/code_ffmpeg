#pragma once

#include "AVConst.h"

/// AVDictionary的RAII包装
class DictWrapper
{
public:
    DictWrapper();

    ~DictWrapper();

public:
    auto get_ptr() -> AVDictionary**;

    auto get() const -> AVDictionary*;

    auto set(const std::string& key, const std::string& value) -> void;

    auto print(const std::string& title) const -> void;

    auto check_unused() const -> void;

private:
    class PImpl;
    std::unique_ptr<PImpl> impl_;
};
