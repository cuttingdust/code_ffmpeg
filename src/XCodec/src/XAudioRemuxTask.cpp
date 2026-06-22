#include "XAudioRemuxTask.h"
#include "XMuxerTask.h"
#include "AVLog.h"

XAudioRemuxTask::XAudioRemuxTask()
{
    setName("AudioRemuxTask");
    LOGD("音频直通任务创建");
}

XAudioRemuxTask::~XAudioRemuxTask()
{
    LOGD("音频直通任务销毁");
}

void XAudioRemuxTask::setMuxer(std::shared_ptr<XMuxerTask> muxer)
{
    muxer_ = std::move(muxer);
}

void XAudioRemuxTask::reset()
{
    XTask::reset();
}

void XAudioRemuxTask::process()
{
    LOGI("音频直通任务开始运行");

    while (!shouldStop())
    {
        auto pkt = popPacket();
        if (!pkt)
        {
            if (eof_reached_ && packet_queue_.empty())
            {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        if (auto muxer = muxer_.lock())
        {
            muxer->pushAudioPacket(std::move(pkt));
        }
    }

    LOGI("音频直通任务结束");
}

IMPLEMENT_CREATE(XAudioRemuxTask)
