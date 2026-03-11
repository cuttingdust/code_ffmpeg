#include "NALAnalyzer.h"

extern "C" {
#include <libavcodec/packet.h>
}

auto NALAnalyzer::NALUnit::type_name() const -> std::string
{
    switch (type)
    {
        case 1:
            return "非IDR切片";
        case 5:
            return "IDR切片";
        case 6:
            return "SEI";
        case 7:
            return "SPS";
        case 8:
            return "PPS";
        case 9:
            return "访问单元分隔符";
        default:
            return "未知类型(" + std::to_string(type) + ")";
    }
}

auto NALAnalyzer::NALUnit::is_keyframe() const -> bool
{
    return type == 5;
}

auto NALAnalyzer::NALUnit::is_sps() const -> bool
{
    return type == 7;
}

auto NALAnalyzer::NALUnit::is_pps() const -> bool
{
    return type == 8;
}

auto NALAnalyzer::NALUnit::is_sei() const -> bool
{
    return type == 6;
}

auto NALAnalyzer::analyze(const AVPacket *pkt) -> std::vector<NALUnit>
{
    std::vector<NALUnit> nals;
    if (!pkt || pkt->size < 4)
    {
        return nals;
    }


    const uint8_t *data   = pkt->data;
    int            size   = pkt->size;
    int            offset = 0;

    while (offset < size - 4)
    {
        if (data[offset] == 0 && data[offset + 1] == 0 && data[offset + 2] == 0 && data[offset + 3] == 1)
        {
            int nal_start = offset + 4;
            if (nal_start < size)
            {
                uint8_t nal_head = data[nal_start];

                NALUnit nal;
                nal.type      = nal_head & 0x1F;
                nal.ref_idc   = (nal_head >> 5) & 0x03;
                nal.forbidden = (nal_head >> 7) & 0x01;
                nal.offset    = offset;

                int next_offset = nal_start + 1;
                while (next_offset < size - 4)
                {
                    if (data[next_offset] == 0 && data[next_offset + 1] == 0 && data[next_offset + 2] == 0 &&
                        data[next_offset + 3] == 1)
                    {
                        break;
                    }
                    next_offset++;
                }
                nal.size = next_offset - nal_start;

                nals.push_back(nal);
            }
            offset += 4;
        }
        else
        {
            offset++;
        }
    }
    return nals;
}

auto NALAnalyzer::print_nal_summary(const std::vector<NALUnit> &nals) -> void
{
    for (size_t i = 0; i < nals.size(); i++)
    {
        if (i > 0)
            std::cout << ",";
        std::cout << nals[i].type;
    }
}

auto NALAnalyzer::print_nal_details(const std::vector<NALUnit> &nals) -> void
{
    for (size_t i = 0; i < nals.size(); i++)
    {
        const auto &nal = nals[i];
        std::cout << "\n    NALU[" << (i + 1) << "]: 类型=" << std::setw(2) << nal.type << " (" << nal.type_name()
                  << ")"
                  << " 大小=" << nal.size << " 优先级=" << nal.ref_idc;
    }
}
