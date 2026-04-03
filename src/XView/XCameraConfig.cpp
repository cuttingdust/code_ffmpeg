#include "XCameraConfig.h"

#include <fstream>

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

auto XCameraConfig::save(const char *path) -> bool
{
    if (!path)
    {
        return false;
    }

    std::ofstream ofs(path, std::ios::binary);
    if (!ofs)
    {
        return false;
    }

    std::scoped_lock lock(mtx_);
    for (auto cam : cams_)
    {
        ofs.write(reinterpret_cast<char *>(&cam), sizeof(cam));
    }
    ofs.close();
    return true;
}

auto XCameraConfig::load(const char *path) -> bool
{
    if (!path)
    {
        return false;
    }

    std::ifstream ifs(path, std::ios::binary);
    if (!ifs)
    {
        return false;
    }

    XCameraData      data;
    std::scoped_lock lock(mtx_);
    cams_.clear();
    for (;;)
    {
        ifs.read(reinterpret_cast<char *>(&data), sizeof(data));
        if (ifs.gcount() != sizeof(data))
        {
            ifs.close();
            return true;
        }
        cams_.push_back(data);
    }
    ifs.close();
    return true;
}

XCameraConfig::XCameraConfig() = default;
