#include "ParserWrapper.h"
#include "AVException.h"

class ParserWrapper::PImpl
{
public:
    explicit PImpl(ParserWrapper* owner, AVCodecID codec_id);

    ~PImpl();

public:
    ParserWrapper*        owner_  = nullptr;
    AVCodecParserContext* parser_ = nullptr;
};

ParserWrapper::ParserWrapper(AVCodecID codec_id) : impl_(std::make_unique<PImpl>(this, codec_id))
{
}

ParserWrapper::PImpl::PImpl(ParserWrapper* owner, AVCodecID codec_id) : owner_(owner)
{
    parser_ = av_parser_init(codec_id);
    if (!parser_)
    {
        throw AVException("无法创建解析器");
    }
}

ParserWrapper::PImpl::~PImpl()
{
    if (parser_)
    {
        av_parser_close(parser_);
    }
}

ParserWrapper::~ParserWrapper() = default;

auto ParserWrapper::get() const -> AVCodecParserContext*
{
    return impl_->parser_;
}

auto ParserWrapper::operator->() const -> AVCodecParserContext*
{
    return impl_->parser_;
}

ParserWrapper::operator AVCodecParserContext*() const
{
    return impl_->parser_;
}

auto ParserWrapper::parse(AVCodecContext* ctx, AVPacket* pkt, const uint8_t* data, int data_size) -> int
{
    return av_parser_parse2(impl_->parser_, ctx, &pkt->data, &pkt->size, data, data_size, AV_NOPTS_VALUE,
                            AV_NOPTS_VALUE, 0);
}
