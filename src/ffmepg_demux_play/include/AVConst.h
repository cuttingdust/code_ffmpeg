#pragma once

#include <iostream>
#include <iomanip>
#include <memory>
#include <string>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/pixfmt.h>
#include <libavformat/avformat.h>
}


struct AVFrame;
struct AVPacket;

struct AVDictionary;
enum AVCodecID;
