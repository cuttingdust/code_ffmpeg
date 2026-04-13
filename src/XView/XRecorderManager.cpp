#include "XRecorderManager.h"
#include "XCameraConfig.h"
#include <filesystem>

namespace fs = std::filesystem;

XRecorderManager& XRecorderManager::instance()
{
    static XRecorderManager manager;
    return manager;
}

void XRecorderManager::registerCallback(StatusCallback callback)
{
    std::lock_guard<std::mutex> lock(callback_mtx_);
    callbacks_.push_back(std::move(callback));
}

void XRecorderManager::notifyStatusChanged(int camera_id, bool is_recording)
{
    // 复制回调列表，避免在调用时被修改
    std::vector<StatusCallback> callbacks_copy;
    {
        std::lock_guard<std::mutex> lock(callback_mtx_);
        callbacks_copy = callbacks_;
    }

    for (const auto& cb : callbacks_copy)
    {
        if (cb)
        {
            cb(camera_id, is_recording);
        }
    }
}

bool XRecorderManager::startRecording(int camera_id, const EncoderConfig& config)
{
    // 先检查是否已在录制（需要锁）
    bool already_recording = false;
    {
        std::scoped_lock lock(mtx_);
        if (recorders_.contains(camera_id))
        {
            LOGI("摄像机 " << camera_id << " 已在录制中");
            already_recording = true;
        }
    }

    if (already_recording)
    {
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

    if (!recorder->startSegmentRecording(prefix, 10, 0))
    {
        LOGE("启动录制失败: " << cam->name);
        return false;
    }

    // 添加到记录器（需要锁）
    {
        std::scoped_lock lock(mtx_);
        recorders_[camera_id] = recorder;
    }

    LOGI("开始录制摄像机 " << camera_id << ": " << cam->name);

    // ✅ 在锁外通知状态变化
    notifyStatusChanged(camera_id, true);

    return true;
}

void XRecorderManager::stopRecording(int camera_id)
{
    // 先取出 recorder 并移除（需要锁）
    std::shared_ptr<RecordClient> recorder;
    {
        std::scoped_lock lock(mtx_);
        auto             it = recorders_.find(camera_id);
        if (it != recorders_.end())
        {
            recorder = it->second;
            recorders_.erase(it);
        }
    }

    if (recorder)
    {
        recorder->stopRecording();
        LOGI("停止录制摄像机 " << camera_id);

        // ✅ 在锁外通知状态变化
        notifyStatusChanged(camera_id, false);
    }
}

void XRecorderManager::stopAll()
{
    // 复制所有 recorder
    std::vector<std::shared_ptr<RecordClient>> all_recorders;
    {
        std::scoped_lock lock(mtx_);
        for (auto& pair : recorders_)
        {
            all_recorders.push_back(pair.second);
        }
        recorders_.clear();
    }

    for (auto& recorder : all_recorders)
    {
        if (recorder)
        {
            recorder->stopRecording();
        }
    }

    LOGI("停止所有录制");
}

bool XRecorderManager::isRecording(int camera_id) const
{
    std::scoped_lock lock(mtx_);
    return recorders_.contains(camera_id);
}
