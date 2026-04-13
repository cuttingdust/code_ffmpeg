#include "XRecorderManager.h"
#include "XCameraConfig.h"
#include <filesystem>

namespace fs = std::filesystem;

XRecorderManager& XRecorderManager::instance()
{
    static XRecorderManager manager;
    return manager;
}

bool XRecorderManager::startRecording(int camera_id, const EncoderConfig& config)
{
    std::scoped_lock lock(mtx_);

    if (recorders_.contains(camera_id))
    {
        LOGI("摄像机 " << camera_id << " 已在录制中");
        return true;
    }

    auto config_ptr = XCameraConfig::instance();
    auto cam        = config_ptr->getCamera(camera_id);
    if (!cam)
    {
        LOGE("摄像机 " << camera_id << " 不存在");
        return false;
    }

    /// 构建保存路径
    std::string save_path = cam->save_path;
    if (save_path.empty())
    {
        save_path = "./recordings";
    }

    fs::path path = fs::path(save_path) / std::to_string(camera_id);
    fs::create_directories(path);

    std::string prefix = path.string() + "/" + cam->name + "_";

    auto recorder = RecordClient::create();
    recorder->setUrl(cam->url);
    recorder->setEncodeConfig(config);
    recorder->setReconnectInterval(5);
    recorder->setMaxReconnects(3);

    if (!recorder->startSegmentRecording(prefix, 60, 0))
    {
        LOGE("启动录制失败: " << cam->name);
        return false;
    }

    recorders_[camera_id] = recorder;
    LOGI("开始录制摄像机 " << camera_id << ": " << cam->name);
    return true;
}

void XRecorderManager::stopRecording(int camera_id)
{
    std::scoped_lock lock(mtx_);

    auto it = recorders_.find(camera_id);
    if (it != recorders_.end())
    {
        it->second->stopRecording();
        recorders_.erase(it);
        LOGI("停止录制摄像机 " << camera_id);
    }
}

void XRecorderManager::stopAll()
{
    std::scoped_lock lock(mtx_);

    for (auto& pair : recorders_)
    {
        pair.second->stopRecording();
    }
    recorders_.clear();
    LOGI("停止所有录制");
}

bool XRecorderManager::isRecording(int camera_id) const
{
    std::scoped_lock lock(mtx_);
    return recorders_.contains(camera_id);
}
