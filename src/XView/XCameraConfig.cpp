#include "XCameraConfig.h"

XCameraConfig *XCameraConfig::instance()
{
    static XCameraConfig config;
    return &config;
}

auto XCameraConfig::addCamera(const XCameraData &cam) -> void
{
    std::scoped_lock lock(mtx_);
    cams_.push_back(cam);
}

auto XCameraConfig::getCameras() -> std::vector<XCameraData>
{
    std::scoped_lock lock(mtx_);
    return cams_;
}

auto XCameraConfig::getCamera(int index) -> XCameraData::Opt
{
    std::scoped_lock lock(mtx_);
    if (index < 0 || index >= cams_.size())
    {
        return std::nullopt;
    }

    return std::make_optional(cams_[index]);
}

auto XCameraConfig::updateCamera(int index, const XCameraData &data) -> bool
{
    std::scoped_lock lock(mtx_);
    if (index < 0 || index >= cams_.size())
    {
        return false;
    }

    cams_[index] = data;
    return true;
}

auto XCameraConfig::deleteCamera(int index) -> bool
{
    std::scoped_lock lock(mtx_);
    if (index < 0 || index >= cams_.size())
    {
        return false;
    }

    cams_.erase(cams_.begin() + index);
    return true;
}

auto XCameraConfig::getCameraCount() -> int
{
    std::scoped_lock lock(mtx_);
    return static_cast<int>(cams_.size());
}

XCameraConfig::XCameraConfig() = default;
