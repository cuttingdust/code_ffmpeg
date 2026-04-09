#pragma once

#include "XTask.h"
#include "VideoEncoder.h"
#include "EncoderConfig.h"

/// 编码任务类
class XEncodeTask : public XTask
{
    DECLARE_CREATE(XEncodeTask)

public:
    XEncodeTask();
    ~XEncodeTask() override;

    // ==================== 编码器控制 ====================

    /// 初始化编码器
    /// \param config 编码器配置
    /// \return 成功返回true
    bool init(const EncoderConfig& config);

    /// 关闭编码器
    void close();

    /// 是否已初始化
    bool isInitialized() const
    {
        return encoder_ != nullptr;
    }

    /// 获取编码器上下文（用于封装）
    AVCodecContext* getCodecContext() const;

    /// 获取编码器配置
    const EncoderConfig& getConfig() const
    {
        return config_;
    }

    // ==================== 任务重置 ====================

    void reset() override;

    void flush();

protected:
    void process() override;

private:
    std::unique_ptr<VideoEncoder> encoder_;
    EncoderConfig                 config_;
    bool                          initialized_ = false;
};
