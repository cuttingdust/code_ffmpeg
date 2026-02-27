#pragma once

#include <iostream>
#include <iomanip>
#include <memory>
#include <string>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/pixfmt.h>
}


struct AVCodecContext;

struct AVFrame;
struct AVPacket;

struct AVDictionary;
enum AVCodecID;
enum AVPixelFormat;
