#include "AudioAtempoFilter.h"

#include "AVException.h"
#include "AVLog.h"
#include "ChannelLayoutWrapper.h"

extern "C" {
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/channel_layout.h>
#include <libavutil/frame.h>
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>
}

#include <cmath>
#include <mutex>
#include <sstream>

class AudioAtempoFilter::PImpl
{
public:
    auto closeGraph() -> void
    {
        tempo_filters_.clear();
        src_ctx_  = nullptr;
        sink_ctx_ = nullptr;
        if (graph_)
        {
            avfilter_graph_free(&graph_);
            graph_ = nullptr;
        }
    }

    auto makeAbufferArgs() const -> std::string
    {
        const ChannelLayoutWrapper layout =
                channels_ == 1 ? ChannelLayoutWrapper::mono() : ChannelLayoutWrapper::stereo();

        char layout_desc[128] = {};
        av_channel_layout_describe(layout.get(), layout_desc, sizeof(layout_desc));

        std::ostringstream oss;
        oss << "time_base=1/" << sample_rate_ << ":sample_rate=" << sample_rate_
            << ":sample_fmt=" << av_get_sample_fmt_name(static_cast<AVSampleFormat>(sample_fmt_))
            << ":channel_layout=" << layout_desc;
        return oss.str();
    }

    auto makeAformatArgs() const -> std::string
    {
        std::ostringstream oss;
        oss << "sample_fmts=" << av_get_sample_fmt_name(static_cast<AVSampleFormat>(sample_fmt_))
            << ":sample_rates=" << sample_rate_ << ":channel_layouts="
            << (channels_ == 1 ? "mono" : "stereo");
        return oss.str();
    }

    auto buildGraph() -> bool
    {
        closeGraph();

        if (tempo_chain_.empty())
        {
            return true;
        }

        graph_ = avfilter_graph_alloc();
        if (!graph_)
        {
            LOGE("AudioAtempoFilter: avfilter_graph_alloc 失败");
            return false;
        }

        const AVFilter* abuffer_f = avfilter_get_by_name("abuffer");
        const AVFilter* atempo_f  = avfilter_get_by_name("atempo");
        const AVFilter* aformat_f = avfilter_get_by_name("aformat");
        const AVFilter* sink_f    = avfilter_get_by_name("abuffersink");
        if (!abuffer_f || !atempo_f || !aformat_f || !sink_f)
        {
            LOGE("AudioAtempoFilter: 找不到 abuffer/atempo/aformat/abuffersink");
            closeGraph();
            return false;
        }

        const std::string abuffer_args = makeAbufferArgs();
        if (avfilter_graph_create_filter(&src_ctx_, abuffer_f, "in", abuffer_args.c_str(), nullptr, graph_)
            < 0)
        {
            LOGE("AudioAtempoFilter: 创建 abuffer 失败, args=" << abuffer_args);
            closeGraph();
            return false;
        }

        AVFilterContext* prev = src_ctx_;
        for (std::size_t i = 0; i < tempo_chain_.size(); ++i)
        {
            AVFilterContext* tempo_ctx = nullptr;
            const std::string args     = "tempo=" + std::to_string(tempo_chain_[i]);
            if (avfilter_graph_create_filter(&tempo_ctx, atempo_f, nullptr, args.c_str(), nullptr, graph_)
                < 0)
            {
                LOGE("AudioAtempoFilter: 创建 atempo 失败 tempo=" << tempo_chain_[i]);
                closeGraph();
                return false;
            }
            if (avfilter_link(prev, 0, tempo_ctx, 0) < 0)
            {
                LOGE("AudioAtempoFilter: link atempo 失败");
                closeGraph();
                return false;
            }
            tempo_filters_.push_back(tempo_ctx);
            prev = tempo_ctx;
        }

        AVFilterContext* aformat_ctx = nullptr;
        const std::string aformat_args = makeAformatArgs();
        if (avfilter_graph_create_filter(&aformat_ctx, aformat_f, "aformat", aformat_args.c_str(), nullptr,
                                         graph_)
            < 0)
        {
            LOGE("AudioAtempoFilter: 创建 aformat 失败, args=" << aformat_args);
            closeGraph();
            return false;
        }
        if (avfilter_link(prev, 0, aformat_ctx, 0) < 0)
        {
            LOGE("AudioAtempoFilter: link aformat 失败");
            closeGraph();
            return false;
        }
        prev = aformat_ctx;

        if (avfilter_graph_create_filter(&sink_ctx_, sink_f, "out", nullptr, nullptr, graph_) < 0)
        {
            LOGE("AudioAtempoFilter: 创建 abuffersink 失败");
            closeGraph();
            return false;
        }

        if (avfilter_link(prev, 0, sink_ctx_, 0) < 0)
        {
            LOGE("AudioAtempoFilter: link abuffersink 失败");
            closeGraph();
            return false;
        }

        if (avfilter_graph_config(graph_, nullptr) < 0)
        {
            LOGE("AudioAtempoFilter: avfilter_graph_config 失败");
            closeGraph();
            return false;
        }

        LOGI("AudioAtempoFilter 图已建立: " << speed_ << "x, atempo 链=" << tempo_chain_.size());
        return true;
    }

    auto drainSink(std::vector<AVFrame*>& out_frames) -> int
    {
        if (!sink_ctx_)
        {
            return 0;
        }

        int count = 0;
        while (true)
        {
            AVFrame* out_frame = av_frame_alloc();
            if (!out_frame)
            {
                throw AVException("AudioAtempoFilter: av_frame_alloc 失败");
            }

            const int ret    = av_buffersink_get_frame(sink_ctx_, out_frame);
            const int eagain = AVERROR(EAGAIN);
            if (ret == eagain || ret == AVERROR_EOF)
            {
                av_frame_free(&out_frame);
                break;
            }
            if (ret < 0)
            {
                av_frame_free(&out_frame);
                throw AVException("AudioAtempoFilter: abuffersink_get_frame 失败", ret);
            }

            out_frames.push_back(out_frame);
            ++count;
        }
        return count;
    }

    auto stampOutputFrames(const AVFrame* in, std::vector<AVFrame*>& out_frames) -> void
    {
        if (!in || out_frames.empty())
        {
            return;
        }

        double pts_sec = 0.0;
        if (in->pts != AV_NOPTS_VALUE)
        {
            pts_sec = in->pts * av_q2d(in->time_base);
        }

        for (AVFrame* out : out_frames)
        {
            out->time_base = in->time_base;
            if (in->pts != AV_NOPTS_VALUE)
            {
                out->pts = in->pts;
            }
            else if (sample_rate_ > 0)
            {
                out->pts = av_rescale_q(static_cast<int64_t>(pts_sec * AV_TIME_BASE),
                                        AV_TIME_BASE_Q,
                                        in->time_base);
                pts_sec += static_cast<double>(out->nb_samples) / static_cast<double>(sample_rate_);
            }
        }
    }

    int                sample_rate_ = 0;
    int                channels_    = 0;
    int                sample_fmt_  = AV_SAMPLE_FMT_S16;
    double             speed_       = 1.0;
    bool               open_        = false;
    bool               bypass_      = true;
    AVFilterGraph*     graph_       = nullptr;
    AVFilterContext*   src_ctx_     = nullptr;
    AVFilterContext*   sink_ctx_    = nullptr;
    std::vector<AVFilterContext*> tempo_filters_;
    std::vector<double>           tempo_chain_;
    std::mutex                    mtx_;
};

AudioAtempoFilter::AudioAtempoFilter() : impl_(std::make_unique<PImpl>())
{
}

AudioAtempoFilter::~AudioAtempoFilter()
{
    close();
}

auto AudioAtempoFilter::open(int sample_rate, int channels, int sample_fmt) -> bool
{
    std::lock_guard<std::mutex> lock(impl_->mtx_);

    impl_->closeGraph();
    impl_->open_        = false;
    impl_->bypass_      = true;
    impl_->speed_       = 1.0;
    impl_->tempo_chain_.clear();

    if (sample_rate <= 0 || channels <= 0 || sample_fmt < 0)
    {
        LOGE("AudioAtempoFilter::open 参数无效");
        return false;
    }

    impl_->sample_rate_ = sample_rate;
    impl_->channels_    = channels;
    impl_->sample_fmt_  = sample_fmt;
    impl_->speed_       = 1.0;
    impl_->bypass_      = true;
    impl_->open_        = true;
    return true;
}

auto AudioAtempoFilter::close() -> void
{
    std::lock_guard<std::mutex> lock(impl_->mtx_);
    impl_->closeGraph();
    impl_->open_   = false;
    impl_->bypass_ = true;
    impl_->speed_  = 1.0;
    impl_->tempo_chain_.clear();
}

auto AudioAtempoFilter::buildTempoChain(double speed) const -> std::vector<double>
{
    std::vector<double> chain;
    if (speed <= 0.0)
    {
        speed = 1.0;
    }

    double remaining = speed;
    while (remaining < 0.5 - 1e-9)
    {
        chain.push_back(0.5);
        remaining /= 0.5;
    }
    while (remaining > 2.0 + 1e-9)
    {
        chain.push_back(2.0);
        remaining /= 2.0;
    }
    chain.push_back(remaining);
    return chain;
}

auto AudioAtempoFilter::setSpeed(double speed) -> void
{
    std::lock_guard<std::mutex> lock(impl_->mtx_);

    if (speed <= 0.0)
    {
        speed = 1.0;
    }

    if (!impl_->open_)
    {
        impl_->speed_ = speed;
        return;
    }

    const bool new_bypass = std::abs(speed - 1.0) < 1e-6;
    const auto new_chain  = buildTempoChain(speed);

    if (new_bypass)
    {
        impl_->closeGraph();
        impl_->bypass_       = true;
        impl_->speed_        = 1.0;
        impl_->tempo_chain_.clear();
        LOGI("AudioAtempoFilter bypass（1.0x）");
        return;
    }

    const bool chain_changed = new_chain != impl_->tempo_chain_;
    impl_->speed_            = speed;
    impl_->tempo_chain_      = new_chain;

    if (chain_changed || !impl_->graph_)
    {
        if (!impl_->buildGraph())
        {
            impl_->bypass_ = true;
            impl_->closeGraph();
            LOGE("AudioAtempoFilter 建图失败, speed=" << speed);
            return;
        }
    }
    else
    {
        for (std::size_t i = 0; i < impl_->tempo_filters_.size(); ++i)
        {
            av_opt_set_double(impl_->tempo_filters_[i], "tempo", impl_->tempo_chain_[i],
                              AV_OPT_SEARCH_CHILDREN);
        }
    }

    impl_->bypass_ = false;
    LOGI("AudioAtempoFilter 倍速=" << speed << "x");
}

auto AudioAtempoFilter::speed() const -> double
{
    return impl_->speed_;
}

auto AudioAtempoFilter::isOpen() const -> bool
{
    return impl_->open_;
}

auto AudioAtempoFilter::isBypass() const -> bool
{
    return impl_->bypass_;
}

auto AudioAtempoFilter::resetPipeline() -> void
{
    std::lock_guard<std::mutex> lock(impl_->mtx_);

    if (!impl_->open_ || impl_->bypass_ || impl_->tempo_chain_.empty())
    {
        return;
    }

    if (!impl_->buildGraph())
    {
        impl_->bypass_ = true;
        impl_->closeGraph();
        LOGE("AudioAtempoFilter resetPipeline 建图失败");
    }
}

auto AudioAtempoFilter::flushOutput(std::vector<AVFrame*>& out_frames) -> int
{
    std::lock_guard<std::mutex> lock(impl_->mtx_);

    if (!impl_->open_ || impl_->bypass_ || !impl_->src_ctx_)
    {
        return 0;
    }

    av_buffersrc_add_frame_flags(impl_->src_ctx_, nullptr, 0);
    return impl_->drainSink(out_frames);
}

auto AudioAtempoFilter::process(AVFrame* in, std::vector<AVFrame*>& out_frames) -> int
{
    std::lock_guard<std::mutex> lock(impl_->mtx_);

    if (!impl_->open_ || !in)
    {
        return 0;
    }

    if (impl_->bypass_)
    {
        out_frames.push_back(in);
        return 1;
    }

    if (!impl_->src_ctx_ || !impl_->sink_ctx_)
    {
        LOGE("AudioAtempoFilter::process 滤镜未就绪");
        return -1;
    }

    int ret = av_buffersrc_add_frame_flags(impl_->src_ctx_, in, 0);
    if (ret == AVERROR_EOF)
    {
        LOGW("AudioAtempoFilter abuffer 已 EOF，重建滤镜图后重试");
        if (!impl_->buildGraph())
        {
            LOGE("AudioAtempoFilter::process 重建滤镜图失败");
            return -1;
        }
        ret = av_buffersrc_add_frame_flags(impl_->src_ctx_, in, 0);
    }
    if (ret < 0)
    {
        throw AVException("AudioAtempoFilter: buffersrc_add_frame 失败", ret);
    }

    const int count = impl_->drainSink(out_frames);
    impl_->stampOutputFrames(in, out_frames);
    return count;
}

auto AudioAtempoFilter::rebuildGraph() -> bool
{
    std::lock_guard<std::mutex> lock(impl_->mtx_);
    return impl_->buildGraph();
}

auto AudioAtempoFilter::drainSink(std::vector<AVFrame*>& out_frames) -> int
{
    std::lock_guard<std::mutex> lock(impl_->mtx_);
    return impl_->drainSink(out_frames);
}
