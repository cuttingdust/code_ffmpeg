#include "AudioDecoder.h"

#include "AVException.h"
#include "AVLog.h"

extern "C" {
#include <libavutil/frame.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
}

class AudioDecoder::PImpl
{
public:
    explicit PImpl(AudioDecoder* owner, AudioDecoderConfig cfg) : owner_(owner), config_(std::move(cfg))
    {
    }

    ~PImpl()
    {
        freeSwr();
    }

    auto findDecoder() -> void
    {
        if (!codec_params_)
        {
            throw AVException("音频解码器未设置 codecpar");
        }

        codec_ = avcodec_find_decoder(codec_params_->get()->codec_id);
        if (!codec_)
        {
            throw AVException("找不到音频解码器: " + std::to_string(codec_params_->get()->codec_id));
        }
    }

    auto openCodec() -> void
    {
        ctx_ = std::make_unique<DecoderContextWrapper>(codec_);
        if (avcodec_parameters_to_context(ctx_->get(), codec_params_->get()) < 0)
        {
            throw AVException("avcodec_parameters_to_context 失败");
        }

        ctx_->get()->thread_count = config_.thread_count;

        const int ret = avcodec_open2(ctx_->get(), codec_, nullptr);
        if (ret < 0)
        {
            throw AVException("音频 avcodec_open2 失败", ret);
        }

        if (config_.output_sample_rate <= 0)
        {
            config_.output_sample_rate = ctx_->get()->sample_rate;
        }

        initSwr();
        LOGI("AudioDecoder 打开: " << config_.output_sample_rate << "Hz, " << config_.output_channels << "ch, S16");
    }

    auto initSwr() -> void
    {
        freeSwr();

        AVChannelLayout in_layout{};
        if (av_channel_layout_copy(&in_layout, &ctx_->get()->ch_layout) < 0)
        {
            throw AVException("复制输入声道布局失败");
        }

        AVChannelLayout out_layout = AV_CHANNEL_LAYOUT_STEREO;
        if (config_.output_channels == 1)
        {
            out_layout = AV_CHANNEL_LAYOUT_MONO;
        }

        int ret = swr_alloc_set_opts2(&swr_,
                                      &out_layout,
                                      config_.output_sample_fmt,
                                      config_.output_sample_rate,
                                      &in_layout,
                                      ctx_->get()->sample_fmt,
                                      ctx_->get()->sample_rate,
                                      0,
                                      nullptr);
        av_channel_layout_uninit(&in_layout);

        if (ret < 0 || !swr_)
        {
            throw AVException("swr_alloc_set_opts2 失败", ret);
        }

        ret = swr_init(swr_);
        if (ret < 0)
        {
            throw AVException("swr_init 失败", ret);
        }
    }

    auto freeSwr() -> void
    {
        if (swr_)
        {
            swr_free(&swr_);
            swr_ = nullptr;
        }
    }

    auto resampleFrame(AVFrame* decoded, std::vector<AVFrame*>& out_frames) -> int
    {
        if (!decoded || !swr_)
        {
            return 0;
        }

        const int max_out =
                swr_get_out_samples(swr_, decoded->nb_samples) + config_.output_sample_rate / 10;
        if (max_out <= 0)
        {
            return 0;
        }

        AVFrame* pcm = av_frame_alloc();
        if (!pcm)
        {
            throw AVException("av_frame_alloc 失败");
        }

        pcm->format      = config_.output_sample_fmt;
        pcm->sample_rate = config_.output_sample_rate;
        if (config_.output_channels == 1)
        {
            pcm->ch_layout = AV_CHANNEL_LAYOUT_MONO;
        }
        else
        {
            pcm->ch_layout = AV_CHANNEL_LAYOUT_STEREO;
        }

        pcm->nb_samples = max_out;
        if (av_frame_get_buffer(pcm, 0) < 0)
        {
            av_frame_free(&pcm);
            throw AVException("PCM 帧缓冲分配失败");
        }

        const int out_samples =
                swr_convert(swr_, pcm->data, max_out, const_cast<const uint8_t**>(decoded->data), decoded->nb_samples);
        if (out_samples < 0)
        {
            av_frame_free(&pcm);
            throw AVException("swr_convert 失败", out_samples);
        }
        if (out_samples == 0)
        {
            av_frame_free(&pcm);
            return 0;
        }

        pcm->nb_samples = out_samples;
        pcm->pts        = decoded->pts;
        pcm->time_base  = config_.time_base;

        out_frames.push_back(pcm);
        stats_.frames_decoded++;
        return 1;
    }

    auto receiveAndResample(std::vector<AVFrame*>& out_frames) -> int
    {
        int count = 0;
        while (true)
        {
            decoded_frame_.unref();
            const int ret = avcodec_receive_frame(ctx_->get(), decoded_frame_);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            {
                break;
            }
            if (ret < 0)
            {
                stats_.errors++;
                throw AVException("avcodec_receive_frame 失败", ret);
            }
            count += resampleFrame(decoded_frame_, out_frames);
        }
        return count;
    }

    AudioDecoder*                           owner_ = nullptr;
    AudioDecoderConfig                      config_;
    const AVCodec*                          codec_ = nullptr;
    std::unique_ptr<DecoderContextWrapper>  ctx_;
    std::shared_ptr<CodecParametersWrapper> codec_params_;
    SwrContext*                             swr_ = nullptr;
    FrameWrapper                            decoded_frame_;
    Stats                                   stats_;
};

AudioDecoder::AudioDecoder(const AudioDecoderConfig& cfg) : impl_(std::make_unique<PImpl>(this, cfg))
{
}

AudioDecoder::~AudioDecoder() = default;

auto AudioDecoder::set_parameters(const AVCodecParameters* par) -> bool
{
    if (!par)
    {
        return false;
    }
    impl_->codec_params_ = CodecParametersWrapper::create();
    return impl_->codec_params_->copy_from(par);
}

auto AudioDecoder::set_parameters_from_stream(AVStream* stream) -> bool
{
    if (!stream || !stream->codecpar)
    {
        return false;
    }

    impl_->codec_params_ = std::make_shared<CodecParametersWrapper>();
    if (!impl_->codec_params_->from_stream(stream))
    {
        return false;
    }

    impl_->config_.time_base = stream->time_base;
    if (stream->codecpar->sample_rate > 0)
    {
        impl_->config_.output_sample_rate = stream->codecpar->sample_rate;
    }
    return true;
}

auto AudioDecoder::get_parameters() const -> std::shared_ptr<CodecParametersWrapper>
{
    return impl_->codec_params_;
}

auto AudioDecoder::open() -> void
{
    impl_->findDecoder();
    impl_->openCodec();
    impl_->stats_ = Stats{};
}

auto AudioDecoder::close() -> void
{
    impl_.reset();
}

auto AudioDecoder::get_ctx() const -> AVCodecContext*
{
    return impl_->ctx_ ? impl_->ctx_->get() : nullptr;
}

auto AudioDecoder::decode_packet(const AVPacket* pkt, std::vector<AVFrame*>& out_frames) -> int
{
    if (!impl_->ctx_)
    {
        throw AVException("AudioDecoder 未打开");
    }

    impl_->stats_.packets_processed++;

    const int ret = avcodec_send_packet(impl_->ctx_->get(), pkt);
    if (ret < 0)
    {
        impl_->stats_.errors++;
        throw AVException("音频 send_packet 失败", ret);
    }

    return impl_->receiveAndResample(out_frames);
}

auto AudioDecoder::flush(std::vector<AVFrame*>& out_frames) -> int
{
    if (!impl_->ctx_)
    {
        return 0;
    }

    avcodec_send_packet(impl_->ctx_->get(), nullptr);
    return impl_->receiveAndResample(out_frames);
}

auto AudioDecoder::flushBuffers() -> void
{
    if (impl_->ctx_)
    {
        avcodec_flush_buffers(impl_->ctx_->get());
    }
}

auto AudioDecoder::reset() -> void
{
    close();
}

auto AudioDecoder::output_sample_rate() const -> int
{
    return impl_ ? impl_->config_.output_sample_rate : 0;
}

auto AudioDecoder::output_channels() const -> int
{
    return impl_ ? impl_->config_.output_channels : 0;
}

auto AudioDecoder::output_sample_format() const -> AVSampleFormat
{
    return impl_ ? impl_->config_.output_sample_fmt : AV_SAMPLE_FMT_NONE;
}

auto AudioDecoder::get_stats() const -> Stats
{
    return impl_ ? impl_->stats_ : Stats{};
}
