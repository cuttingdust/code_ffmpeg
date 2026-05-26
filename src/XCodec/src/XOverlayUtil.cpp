#include "XOverlayUtil.h"
#include "XDisplayTask.h"

XOverlayStyle defaultRecOverlayStyle()
{
    return XOverlayStyle{};
}

RecStyle recStyleFromOverlay(const XOverlayStyle& style)
{
    RecStyle rec;
    rec.dot_radius     = style.dot_radius;
    rec.dot_color      = { style.dot_r, style.dot_g, style.dot_b, style.dot_a };
    rec.text_color     = { style.text_r, style.text_g, style.text_b, style.text_a };
    rec.font_size      = style.font_size;
    rec.spacing        = style.spacing;
    rec.padding_top    = style.padding_top;
    rec.padding_bottom = style.padding_bottom;
    rec.padding_left   = style.padding_left;
    rec.padding_right  = style.padding_right;
    rec.show_border    = style.show_border;
    rec.border_color   = { style.border_r, style.border_g, style.border_b, style.border_a };
    return rec;
}

XOverlayStyle overlayStyleFromRec(const RecStyle& rec)
{
    XOverlayStyle style;
    style.dot_radius     = rec.dot_radius;
    style.dot_r          = rec.dot_color.r;
    style.dot_g          = rec.dot_color.g;
    style.dot_b          = rec.dot_color.b;
    style.dot_a          = rec.dot_color.a;
    style.text_r         = rec.text_color.r;
    style.text_g         = rec.text_color.g;
    style.text_b         = rec.text_color.b;
    style.text_a         = rec.text_color.a;
    style.font_size      = rec.font_size;
    style.spacing        = rec.spacing;
    style.padding_top    = rec.padding_top;
    style.padding_bottom = rec.padding_bottom;
    style.padding_left   = rec.padding_left;
    style.padding_right  = rec.padding_right;
    style.show_border    = rec.show_border;
    style.border_r       = rec.border_color.r;
    style.border_g       = rec.border_color.g;
    style.border_b       = rec.border_color.b;
    style.border_a       = rec.border_color.a;
    return style;
}
