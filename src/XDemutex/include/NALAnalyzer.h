#pragma once

#include "AVConst.h"

/// ==================== NALU分析器类 ====================
class NALAnalyzer
{
public:
    struct NALUnit
    {
        int type;
        int ref_idc;
        int forbidden;
        int size;
        int offset;

        auto type_name() const -> std::string;

        auto is_keyframe() const -> bool;

        auto is_sps() const -> bool;

        auto is_pps() const -> bool;

        auto is_sei() const -> bool;
    };

    auto analyze(const AVPacket* pkt) -> std::vector<NALUnit>;

    auto print_nal_summary(const std::vector<NALUnit>& nals) -> void;

    auto print_nal_details(const std::vector<NALUnit>& nals) -> void;
};
