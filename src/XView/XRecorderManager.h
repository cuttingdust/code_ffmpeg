#pragma once

#include "RecordClient.h"
#include <map>
#include <mutex>

class XRecorderManager
{
public:
    static XRecorderManager& instance();

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

    std::map<int, std::shared_ptr<RecordClient>> recorders_;
    mutable std::mutex                           mtx_;
};
