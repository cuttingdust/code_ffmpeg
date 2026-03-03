#include "DictWrapper.h"
#include "AVException.h"

#include <iostream>

extern "C" {
#include <libavutil/dict.h>
}

class DictWrapper::PImpl
{
public:
    PImpl(DictWrapper *owenr);
    ~PImpl() = default;

public:
    DictWrapper  *owenr_ = nullptr;
    AVDictionary *dict_  = NULL;
};

DictWrapper::PImpl::PImpl(DictWrapper *owenr) : owenr_(owenr)
{
}


DictWrapper::DictWrapper() : impl_(std::make_unique<PImpl>(this))
{
}

DictWrapper::~DictWrapper()
{
    if (impl_->dict_)
    {
        av_dict_free(&impl_->dict_);
    }
}

auto DictWrapper::get_ptr() -> AVDictionary **
{
    return &impl_->dict_;
}

auto DictWrapper::get() const -> AVDictionary *
{
    return impl_->dict_;
}

auto DictWrapper::set(const std::string &key, const std::string &value) -> void
{
    int ret = av_dict_set(&impl_->dict_, key.c_str(), value.c_str(), 0);
    if (ret < 0)
    {
        throw AVException("设置参数失败: " + key, ret);
    }
}

auto DictWrapper::print(const std::string &title) const -> void
{
    if (!impl_->dict_)
    {
        return;
    }

    std::cout << title << ":" << std::endl;
    const AVDictionaryEntry *entry = nullptr;
    while ((entry = av_dict_get(impl_->dict_, "", entry, AV_DICT_IGNORE_SUFFIX)))
    {
        std::cout << "  " << entry->key << " = " << entry->value << std::endl;
    }
}

auto DictWrapper::check_unused() const -> void
{
    if (!impl_->dict_)
        return;

    const AVDictionaryEntry *entry      = nullptr;
    bool                     has_unused = false;
    while ((entry = av_dict_get(impl_->dict_, "", entry, AV_DICT_IGNORE_SUFFIX)))
    {
        if (!has_unused)
        {
            std::cout << "\n警告: 以下参数未被编码器接受:" << std::endl;
            has_unused = true;
        }
        std::cout << "  " << entry->key << " = " << entry->value << " (未使用)" << std::endl;
    }
}
