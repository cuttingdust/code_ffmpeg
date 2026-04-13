#pragma once

#include "RecordClient.h"
#include <map>
#include <mutex>
#include <functional>
#include <vector>

class XRecorderManager
{
public:
    static XRecorderManager& instance();

    /// 录制状态变化回调
    using StatusCallback = std::function<void(int camera_id, bool is_recording)>;

    /// 注册录制状态变化回调
    void registerCallback(StatusCallback callback);

    /// 开始录制指定摄像机
    bool startRecording(int camera_id, const EncoderConfig& config);

    /// 停止录制指定摄像机
    void stopRecording(int camera_id);

    /// 停止所有录制
    void stopAll();

    /// 是否正在录制
    bool isRecording(int camera_id) const;

private:
    XRecorderManager() = default;

    void notifyStatusChanged(int camera_id, bool is_recording);

    std::map<int, std::shared_ptr<RecordClient>> recorders_;
    mutable std::mutex                           mtx_;

    std::vector<StatusCallback> callbacks_;
    mutable std::mutex          callback_mtx_;
};
