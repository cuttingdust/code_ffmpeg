#pragma once

#include "XCodec_Global.h"
#include "XOverlayStyle.h"

struct RecStyle;

XCODEC_EXPORT XOverlayStyle defaultRecOverlayStyle();

XCODEC_EXPORT RecStyle      recStyleFromOverlay(const XOverlayStyle& style);
XCODEC_EXPORT XOverlayStyle overlayStyleFromRec(const RecStyle& style);
