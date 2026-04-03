#pragma once
#include <vector>
#include <mutex>
#include <optional>
struct XCameraData
{
    char name[1024]      = { 0 };
    char url[4096]       = { 0 }; ///< 摄像机主码流
    char sub_url[4096]   = { 0 }; ///< 摄像机辅码流
    char save_path[4096] = { 0 }; ///< 视频录制存放目录
    using Opt            = std::optional<XCameraData>;
};

class XCameraConfig
{
public:
    static XCameraConfig *instance();

    auto addCamera(const XCameraData &cam) -> void;

    auto getCameras() -> std::vector<XCameraData>;

    auto getCamera(int index) -> XCameraData::Opt;

    auto updateCamera(int index, const XCameraData &data) -> bool;

    auto deleteCamera(int index) -> bool;

    auto getCameraCount() -> int;

    auto save(const char *path) -> bool;

    auto load(const char *path) -> bool;

private:
    XCameraConfig();

private:
    std::vector<XCameraData> cams_;
    std::mutex               mtx_;
};
