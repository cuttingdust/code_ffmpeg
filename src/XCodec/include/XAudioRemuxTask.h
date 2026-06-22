#pragma once

#include "XTask.h"
#include <memory>

class XMuxerTask;

/// 音频压缩包直通任务：从 Demux 音频支路接收包，转交给 XMuxerTask 写入
class XAudioRemuxTask : public XTask
{
    DECLARE_CREATE(XAudioRemuxTask)

public:
    XAudioRemuxTask();
    ~XAudioRemuxTask() override;

    void setMuxer(std::shared_ptr<XMuxerTask> muxer);

    void reset() override;

protected:
    void process() override;

private:
    std::weak_ptr<XMuxerTask> muxer_;
};
