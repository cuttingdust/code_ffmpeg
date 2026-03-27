#include "FileWriter.h"

#include <fstream>

class FileWriter::PImpl
{
public:
    PImpl(FileWriter *owenr, std::string fname);
    ~PImpl();

public:
    FileWriter   *owenr_ = nullptr;
    std::ofstream file_;
    std::string   filename_;
    size_t        bytes_written_ = 0;
};

FileWriter::PImpl::PImpl(FileWriter *owenr, std::string fname) : owenr_(owenr), filename_(std::move(fname))
{
    file_.open(filename_, std::ios::binary);
    if (!file_.is_open())
    {
        throw std::runtime_error("无法打开文件: " + filename_);
    }
}

FileWriter::PImpl::~PImpl()
{
    if (file_.is_open())
    {
        file_.close();
    }
}

FileWriter::FileWriter(const std::string &fname) : impl_(std::make_unique<PImpl>(this, fname))
{
}

FileWriter::~FileWriter() = default;

auto FileWriter::write_packet(const AVPacket *pkt) -> void
{
    impl_->file_.write(reinterpret_cast<const char *>(pkt->data), pkt->size);
    impl_->bytes_written_ += pkt->size;
}

auto FileWriter::get_size() const -> size_t
{
    return impl_->bytes_written_;
}

auto FileWriter::get_filename() const -> std::string
{
    return impl_->filename_;
}
