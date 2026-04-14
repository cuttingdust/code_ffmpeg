#include "XRecorderManager.h"
#include "XCameraConfig.h"
#include <filesystem>
#include <QDir>
#include <QDateTime>
#include <regex>

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

    // 在锁外通知状态变化
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

        // 在锁外通知状态变化
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

// ========== 录像文件管理实现 ==========

std::string XRecorderManager::getRecordRootPath(int camera_id) const
{
    auto config = XCameraConfig::instance();
    auto cam    = config->getCamera(camera_id);
    if (!cam)
    {
        return "";
    }

    std::string save_path = cam->save_path;
    if (save_path.empty())
    {
        save_path = "./recordings";
    }

    fs::path path = fs::path(save_path) / std::to_string(camera_id);
    return path.string();
}

QDateTime XRecorderManager::parseFilenameToDateTime(const std::string& filename)
{
    // 文件名格式: name_1_20260413_105740.mp4
    std::regex  pattern(R"(\d{8}_\d{6})"); // 匹配 20260413_105740
    std::smatch match;
    std::string name = filename;

    if (std::regex_search(name, match, pattern))
    {
        std::string datetime_str = match[0];
        // datetime_str = "20260413_105740"
        std::string date_str = datetime_str.substr(0, 8); // "20260413"
        std::string time_str = datetime_str.substr(9, 6); // "105740"

        int year   = std::stoi(date_str.substr(0, 4));
        int month  = std::stoi(date_str.substr(4, 2));
        int day    = std::stoi(date_str.substr(6, 2));
        int hour   = std::stoi(time_str.substr(0, 2));
        int minute = std::stoi(time_str.substr(2, 2));
        int second = std::stoi(time_str.substr(4, 2));

        return QDateTime(QDate(year, month, day), QTime(hour, minute, second));
    }

    return QDateTime();
}

std::vector<RecordFileInfo> XRecorderManager::getRecordFiles(int camera_id)
{
    std::vector<RecordFileInfo> result;

    std::string root_path = getRecordRootPath(camera_id);
    if (root_path.empty())
    {
        return result;
    }

    QDir dir(root_path.c_str());
    if (!dir.exists())
    {
        return result;
    }

    // 筛选视频文件
    QStringList filters;
    filters << "*.mp4" << "*.avi" << "*.mkv";
    dir.setNameFilters(filters);

    auto files = dir.entryInfoList(QDir::Files);
    for (const auto& file : files)
    {
        RecordFileInfo info;
        info.filename = file.fileName().toStdString();
        info.path     = file.absoluteFilePath().toStdString();
        info.size     = file.size();
        info.datetime = parseFilenameToDateTime(info.filename);
        info.duration = 10; // 默认10秒分段

        result.push_back(info);
    }

    // 按时间排序（最新的在前）
    std::sort(result.begin(), result.end(),
              [](const RecordFileInfo& a, const RecordFileInfo& b) { return a.datetime > b.datetime; });

    return result;
}

std::vector<QDate> XRecorderManager::getRecordDates(int camera_id)
{
    std::vector<QDate> dates;
    auto               files = getRecordFiles(camera_id);

    for (const auto& file : files)
    {
        if (file.datetime.isValid())
        {
            QDate date = file.datetime.date();
            if (std::find(dates.begin(), dates.end(), date) == dates.end())
            {
                dates.push_back(date);
            }
        }
    }

    // 按日期排序（最新的在前）
    std::sort(dates.begin(), dates.end(), std::greater<QDate>());

    return dates;
}

std::vector<RecordFileInfo> XRecorderManager::getRecordFilesByDate(int camera_id, const QDate& date)
{
    std::vector<RecordFileInfo> result;
    auto                        files = getRecordFiles(camera_id);

    for (const auto& file : files)
    {
        if (file.datetime.isValid() && file.datetime.date() == date)
        {
            result.push_back(file);
        }
    }

    // 按时间排序（最新的在前）
    std::sort(result.begin(), result.end(),
              [](const RecordFileInfo& a, const RecordFileInfo& b) { return a.datetime > b.datetime; });

    return result;
}

std::string XRecorderManager::getRecordFilePath(int camera_id, const std::string& filename)
{
    std::string root_path = getRecordRootPath(camera_id);
    if (root_path.empty())
    {
        return "";
    }

    fs::path path = fs::path(root_path) / filename;
    if (fs::exists(path))
    {
        return path.string();
    }

    return "";
}

bool XRecorderManager::deleteRecordFile(int camera_id, const std::string& filename)
{
    std::string filepath = getRecordFilePath(camera_id, filename);
    if (filepath.empty())
    {
        return false;
    }

    try
    {
        return fs::remove(filepath);
    }
    catch (const std::exception& e)
    {
        LOGE("删除录像文件失败: " << e.what());
        return false;
    }
}
