#pragma once

#include <cstdint>

/// 与 XDisplayTask::RecStyle 默认值对齐的 overlay 样式（不依赖 SDL）
struct XOverlayStyle
{
    int      dot_radius     = 6;
    uint8_t  dot_r          = 255;
    uint8_t  dot_g          = 50;
    uint8_t  dot_b          = 50;
    uint8_t  dot_a          = 255;
    uint8_t  text_r         = 255;
    uint8_t  text_g         = 255;
    uint8_t  text_b         = 255;
    uint8_t  text_a         = 255;
    int      font_size      = 12;
    int      spacing        = 4;
    int      padding_top    = 2;
    int      padding_bottom = 2;
    int      padding_left   = 8;
    int      padding_right  = 8;
    bool     show_border    = true;
    uint8_t  border_r       = 255;
    uint8_t  border_g       = 255;
    uint8_t  border_b       = 255;
    uint8_t  border_a       = 200;
    int      margin_left    = 8;
    int      margin_top     = 8;
};
