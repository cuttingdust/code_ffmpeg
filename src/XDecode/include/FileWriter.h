#pragma once

#include "AVConst.h"

/// ==================== 文件写入器 ====================
class FileWriter
{
private:
public:
    explicit FileWriter(const std::string& fname);

    ~FileWriter();

public:
    void write_packet(const AVPacket* pkt);

    auto get_size() const -> size_t;

    auto get_filename() const -> std::string;

private:
    class PImpl;
    std::unique_ptr<PImpl> impl_;
};
