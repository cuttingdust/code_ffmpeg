// XRecorderManager.h - 添加
#pragma once

#include "RecordClient.h"
#include <map>
#include <mutex>
#include <functional>
#include <vector>
#include <string>
#include <QDateTime>

/// 录像文件信息
struct RecordFileInfo
{
    std::string filename; ///< 完整文件名
    std::string path;     ///< 完整路径
    QDateTime   datetime; ///< 录像时间（从文件名解析）
    int64_t     size;     ///< 文件大小（字节）
    int         duration; ///< 录像时长（秒）
};

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

    // ========== 新增：录像文件管理 ==========

    /// 获取摄像机的录像文件列表（按日期分组）
    /// \param camera_id 摄像机ID
    /// \return 录像文件信息列表
    std::vector<RecordFileInfo> getRecordFiles(int camera_id);

    /// 获取摄像机的录像日期列表（有录像的日期）
    /// \param camera_id 摄像机ID
    /// \return 日期列表
    std::vector<QDate> getRecordDates(int camera_id);

    /// 获取指定日期的录像文件列表
    /// \param camera_id 摄像机ID
    /// \param date 日期
    /// \return 该日期的录像文件列表
    std::vector<RecordFileInfo> getRecordFilesByDate(int camera_id, const QDate& date);

    /// 获取录像文件的完整路径
    /// \param camera_id 摄像机ID
    /// \param filename 文件名
    /// \return 完整路径
    std::string getRecordFilePath(int camera_id, const std::string& filename);

    /// 删除录像文件
    /// \param camera_id 摄像机ID
    /// \param filename 文件名
    /// \return 成功返回true
    bool deleteRecordFile(int camera_id, const std::string& filename);

private:
    XRecorderManager() = default;

    void notifyStatusChanged(int camera_id, bool is_recording);

    /// 解析文件名中的时间
    QDateTime parseFilenameToDateTime(const std::string& filename);

    /// 获取摄像机的录像根目录
    std::string getRecordRootPath(int camera_id) const;

    std::map<int, std::shared_ptr<RecordClient>> recorders_;
    mutable std::mutex                           mtx_;

    std::vector<StatusCallback> callbacks_;
    mutable std::mutex          callback_mtx_;
};
