// This is a personal academic project. Dear PVS-Studio, please check it.

// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

/*
* Copyright (C) 2016 Oka Motofumi <chikuzen.mo at gmail dot com>
*
* This file is part of avs2pipemod.
*
* avs2pipemod is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* avs2pipemod is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with avs2pipemod.  If not, see <http://www.gnu.org/licenses/>.
*
*/


#include <cstdio>
#include <malloc.h>
#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#define NOMINMAX
#define NOGDI
#include <windows.h>
#include <avisynth.h>
#include "utils.h"

void validate(bool cond, const char* msg, ...)
{
    if (!cond) return;

    char buff[256];
    va_list args;

    va_start(args, msg);
    vsnprintf(buff, 256, msg, args);
    va_end(args);

    throw std::runtime_error(buff);
}


void a2pm_log(int level, const char *message, ...)
{
    va_list args;
    fprintf(stderr,
        level == LOG_INFO ? "avs2pipemod[info]: " :
        level == LOG_REPEAT ? "\ravs2pipemod[info]: " :
        level == LOG_WARNING ? "avs2pipemod[warning]: " : "");
        level == LOG_INFO ? "avs2pipemod [info]: " :
        level == LOG_REPEAT ? "\ravs2pipemod [info]: " :
        level == LOG_WARNING ? "avs2pipemod [warning]: " : "");

    va_start(args, message);
    vfprintf(stderr, message, args);
    va_end(args);
}


const char* get_string_info(int pix_type)
{
    switch (pix_type) {
    // gray
    case VideoInfo::CS_Y8:          return "Y8";
    case VideoInfo::CS_Y10:         return "Y10";
    case VideoInfo::CS_Y12:         return "Y12";
    case VideoInfo::CS_Y14:         return "Y14";
    case VideoInfo::CS_Y16:         return "Y16";
    case VideoInfo::CS_Y32:         return "Y32";
    // packed RGB
    case VideoInfo::CS_BGR32:       return "RGB32";
    case VideoInfo::CS_BGR64:       return "RGB64";
    case VideoInfo::CS_BGR24:       return "RGB24";
    case VideoInfo::CS_BGR48:       return "RGB48";
    // packed YUV
    case VideoInfo::CS_YUY2:        return "YUY2";
    // planar RGB
    case VideoInfo::CS_RGBP:        return "RGBP";
    case VideoInfo::CS_RGBP10:      return "RGBP10";
    case VideoInfo::CS_RGBP12:      return "RGBP12";
    case VideoInfo::CS_RGBP14:      return "RGBP14";
    case VideoInfo::CS_RGBP16:      return "RGBP16";
    case VideoInfo::CS_RGBPS:       return "RGBPS";
    // planar RGB with alpha
    case VideoInfo::CS_RGBAP:       return "RGBAP";
    case VideoInfo::CS_RGBAP10:     return "RGBAP10";
    case VideoInfo::CS_RGBAP12:     return "RGBAP12";
    case VideoInfo::CS_RGBAP14:     return "RGBAP14";
    case VideoInfo::CS_RGBAP16:     return "RGBAP16";
    case VideoInfo::CS_RGBAPS:      return "RGBAPS";
    // planar yuv444
    case VideoInfo::CS_YV24:        return "YV24";
    case VideoInfo::CS_YUV444P10:   return "YUV444P10";
    case VideoInfo::CS_YUV444P12:   return "YUV444P12";
    case VideoInfo::CS_YUV444P14:   return "YUV444P14";
    case VideoInfo::CS_YUV444P16:   return "YUV444P16";
    case VideoInfo::CS_YUV444PS:    return "YUV444PS";
    // planar yuv444 with alpha
    case VideoInfo::CS_YUVA444:     return "YUVA444";
    case VideoInfo::CS_YUVA444P10:  return "YUVA444P10";
    case VideoInfo::CS_YUVA444P12:  return "YUVA444P12";
    case VideoInfo::CS_YUVA444P14:  return "YUVA444P14";
    case VideoInfo::CS_YUVA444P16:  return "YUVA444P16";
    case VideoInfo::CS_YUVA444PS:   return "YUVA444PS";
    // planar yuv422
    case VideoInfo::CS_YV16:        return "YV16";
    case VideoInfo::CS_YUV422P10:   return "YUV422P10";
    case VideoInfo::CS_YUV422P12:   return "YUV422P12";
    case VideoInfo::CS_YUV422P14:   return "YUV422P14";
    case VideoInfo::CS_YUV422P16:   return "YUV422P16";
    case VideoInfo::CS_YUV422PS:    return "YUV422PS";
    // planar yuv422 with alpha
    case VideoInfo::CS_YUVA422:     return "YUVA422";
    case VideoInfo::CS_YUVA422P10:  return "YUVA422P10";
    case VideoInfo::CS_YUVA422P12:  return "YUVA422P12";
    case VideoInfo::CS_YUVA422P14:  return "YUVA422P14";
    case VideoInfo::CS_YUVA422P16:  return "YUVA422P16";
    case VideoInfo::CS_YUVA422PS:   return "YUVA422PS";
    // planar yuv420
    case VideoInfo::CS_YV12:
    case VideoInfo::CS_I420:        return "YV12";
    case VideoInfo::CS_YUV420P10:   return "YUV420P10";
    case VideoInfo::CS_YUV420P12:   return "YUV420P12";
    case VideoInfo::CS_YUV420P14:   return "YUV420P14";
    case VideoInfo::CS_YUV420P16:   return "YUV420P16";
    case VideoInfo::CS_YUV420PS:    return "YUV420PS";
    // planar yuv420 with alpha
    case VideoInfo::CS_YUVA420:     return "YUVA420";
    case VideoInfo::CS_YUVA420P10:  return "YUVA420P10";
    case VideoInfo::CS_YUVA420P12:  return "YUVA420P12";
    case VideoInfo::CS_YUVA420P14:  return "YUVA420P14";
    case VideoInfo::CS_YUVA420P16:  return "YUVA420P16";
    case VideoInfo::CS_YUVA420PS:   return "YUVA420PS";
    // planar yuv411
    case VideoInfo::CS_YV411:       return "YV411";
    // others
    default: return "unknown";
    }
}


const char* get_string_video_out(int pix_type)
{
    switch (pix_type) {
    // gray
    case VideoInfo::CS_Y8:          return "luma-only-8bit";
    case VideoInfo::CS_Y10:         return "luma-only-10bit";
    case VideoInfo::CS_Y12:         return "luma-only-12bit";
    case VideoInfo::CS_Y14:         return "luma-only-14bit";
    case VideoInfo::CS_Y16:         return "luma-only-16bit";
    case VideoInfo::CS_Y32:         return "luma-only-float";
    // packed RGB
    case VideoInfo::CS_BGR32:       return "BGRA-packed-8bit";
    case VideoInfo::CS_BGR64:       return "BGRA-packed-16bit";
    case VideoInfo::CS_BGR24:       return "BGR-packed-8bit";
    case VideoInfo::CS_BGR48:       return "BGR-packed-16bit";
    // packed YUV
    case VideoInfo::CS_YUY2:        return "YUYV-packed-8bit";
    // planar RGB
    case VideoInfo::CS_RGBP:        return "GBR-planar-8bit";
    case VideoInfo::CS_RGBP10:      return "GBR-planar-10bit";
    case VideoInfo::CS_RGBP12:      return "GBR-planar-12bit";
    case VideoInfo::CS_RGBP14:      return "GBR-planar-14bit";
    case VideoInfo::CS_RGBP16:      return "GBR-planar-16bit";
    case VideoInfo::CS_RGBPS:       return "GBR-planar-float";
    // planar RGB with alpha
    case VideoInfo::CS_RGBAP:       return "GBRA-planar-8bit";
    case VideoInfo::CS_RGBAP10:     return "GBRA-planar-10bit";
    case VideoInfo::CS_RGBAP12:     return "GBRA-planar-12bit";
    case VideoInfo::CS_RGBAP14:     return "GBRA-planar-14bit";
    case VideoInfo::CS_RGBAP16:     return "GBRA-planar-16bit";
    case VideoInfo::CS_RGBAPS:      return "GBRA-planar-float";
    // planar yuv444
    case VideoInfo::CS_YV24:        return "YUV-444-planar-8bit";
    case VideoInfo::CS_YUV444P10:   return "YUV-444-planar-10bit";
    case VideoInfo::CS_YUV444P12:   return "YUV-444-planar-12bit";
    case VideoInfo::CS_YUV444P14:   return "YUV-444-planar-14bit";
    case VideoInfo::CS_YUV444P16:   return "YUV-444-planar-16bit";
    case VideoInfo::CS_YUV444PS:    return "YUV-444-planar-float";
    // planar yuv444 with alpha
    case VideoInfo::CS_YUVA444:     return "YUVA-444-planar-8bit";
    case VideoInfo::CS_YUVA444P10:  return "YUVA-444-planar-10bit";
    case VideoInfo::CS_YUVA444P12:  return "YUVA-444-planar-12bit";
    case VideoInfo::CS_YUVA444P14:  return "YUVA-444-planar-14bit";
    case VideoInfo::CS_YUVA444P16:  return "YUVA-444-planar-16bit";
    case VideoInfo::CS_YUVA444PS:   return "YUVA-444-planar-float";
    // planar yuv422
    case VideoInfo::CS_YV16:        return "YUV-422-planar-8bit";
    case VideoInfo::CS_YUV422P10:   return "YUV-422-planar-10bit";
    case VideoInfo::CS_YUV422P12:   return "YUV-422-planar-12bit";
    case VideoInfo::CS_YUV422P14:   return "YUV-422-planar-14bit";
    case VideoInfo::CS_YUV422P16:   return "YUV-422-planar-16bit";
    case VideoInfo::CS_YUV422PS:    return "YUV-422-planar-float";
    // planar yuv422 with alpha
    case VideoInfo::CS_YUVA422:     return "YUVA-422-planar-8bit";
    case VideoInfo::CS_YUVA422P10:  return "YUVA-422-planar-10bit";
    case VideoInfo::CS_YUVA422P12:  return "YUVA-422-planar-12bit";
    case VideoInfo::CS_YUVA422P14:  return "YUVA-422-planar-14bit";
    case VideoInfo::CS_YUVA422P16:  return "YUVA-422-planar-16bit";
    case VideoInfo::CS_YUVA422PS:   return "YUVA-422-planar-float";
    // planar yuv420
    case VideoInfo::CS_YV12:
    case VideoInfo::CS_I420:        return "YUV-420-planar-8bit";
    case VideoInfo::CS_YUV420P10:   return "YUV-420-planar-10bit";
    case VideoInfo::CS_YUV420P12:   return "YUV-420-planar-12bit";
    case VideoInfo::CS_YUV420P14:   return "YUV-420-planar-14bit";
    case VideoInfo::CS_YUV420P16:   return "YUV-420-planar-16bit";
    case VideoInfo::CS_YUV420PS:    return "YUV-420-planar-float";
    // planar yuv420 with alpha
    case VideoInfo::CS_YUVA420:     return "YUVA-420-planar-8bit";
    case VideoInfo::CS_YUVA420P10:  return "YUVA-420-planar-10bit";
    case VideoInfo::CS_YUVA420P12:  return "YUVA-420-planar-12bit";
    case VideoInfo::CS_YUVA420P14:  return "YUVA-420-planar-14bit";
    case VideoInfo::CS_YUVA420P16:  return "YUVA-420-planar-16bit";
    case VideoInfo::CS_YUVA420PS:   return "YUVA-420-planar-float";
    // planar yuv411
    case VideoInfo::CS_YV411:       return "YUV-411-planar-8bit";
    // others
    default: return "unknown";
    }
}


const char* get_string_y4mheader(int pix_type)
{
    switch (pix_type) {
    case VideoInfo::CS_Y8:          return "mono";
    case VideoInfo::CS_Y16:         return "mono16";

    case VideoInfo::CS_YV411:       return "411";

    case VideoInfo::CS_YV12:
    case VideoInfo::CS_I420:        return "420mpeg2";
    case VideoInfo::CS_YUV420P10:   return "420p10";
    case VideoInfo::CS_YUV420P12:   return "420p12";
    case VideoInfo::CS_YUV420P14:   return "420p14";
    case VideoInfo::CS_YUV420P16:   return "420p16";

    case VideoInfo::CS_YV16:        return "422";
    case VideoInfo::CS_YUV422P10:   return "422p10";
    case VideoInfo::CS_YUV422P12:   return "422p12";
    case VideoInfo::CS_YUV422P14:   return "422p14";
    case VideoInfo::CS_YUV422P16:   return "422p16";

    case VideoInfo::CS_YV24:        return "444";
    case VideoInfo::CS_YUV444P10:   return "444p10";
    case VideoInfo::CS_YUV444P12:   return "444p12";
    case VideoInfo::CS_YUV444P14:   return "444p14";
    case VideoInfo::CS_YUV444P16:   return "444p16";

    case VideoInfo::CS_YUVA444:     return "444alpha";

    default: return nullptr;
    }
}


int get_sample_bits(int pixel_type)
{
    int32_t bits = pixel_type & VideoInfo::CS_Sample_Bits_Mask;
    switch (bits) {
    case VideoInfo::CS_Sample_Bits_8:  return 8;
    case VideoInfo::CS_Sample_Bits_10: return 10;
    case VideoInfo::CS_Sample_Bits_12: return 12;
    case VideoInfo::CS_Sample_Bits_14: return 14;
    case VideoInfo::CS_Sample_Bits_16: return 16;
    case VideoInfo::CS_Sample_Bits_32: return 32;
    }
    return 0;
}

int get_num_planes(int pixel_type)
{
    if (pixel_type & VideoInfo::CS_INTERLEAVED) {
        return 1;
    }
    if (pixel_type & VideoInfo::CS_BGR) {
        return (pixel_type & VideoInfo::CS_RGBA_TYPE) ? 4 : 3;
    }
    return (pixel_type & VideoInfo::CS_YUVA) ? 4 : 3;
}

#if 0
const char* get_string_filter(int pix_type)
{
    switch (pix_type) {
    case VideoInfo::CS_BGR32: return "ConvertToYV24";
    case VideoInfo::CS_BGR24: return "ConvertToYV24";
    case VideoInfo::CS_YUY2:  return "ConvertToYV16";
    default: return nullptr;
    }
}


x264raw_t get_string_x264raw(int pix_type)
{
    using std::make_tuple;

    const char* unkown = nullptr;

    switch (pix_type) {
    case VideoInfo::CS_BGR32:
        return make_tuple("raw", "bgra", "rgb");
    case VideoInfo::CS_BGR24:
        return make_tuple("raw", "bgr", "rgb");
    case VideoInfo::CS_YUY2:
        return make_tuple("lavf --input-fmt rawvideo", "yuyv422", "i422");
    case VideoInfo::CS_YV24:
    case VideoInfo::CS_YUV444P16:
        return make_tuple("raw", "i444", "i444");
    case VideoInfo::CS_YV16:
    case VideoInfo::CS_YUV422P16:
        return make_tuple("raw", "i422", "i422");
    case VideoInfo::CS_YV12:
    case VideoInfo::CS_I420:
    case VideoInfo::CS_YUV420P16:
        return make_tuple("raw", "i420", "i420");
    case VideoInfo::CS_Y8:
        return make_tuple("lavf --input-fmt rawvideo", "gray", "i420");
    case VideoInfo::CS_Y16:
        return make_tuple("lavf --input-fmt rawvideo", "gray16", "i420");
    default:
        return make_tuple(nullptr, nullptr, nullptr);
    }
}
#endif

Buffer::Buffer(size_t size, size_t align)
{
    buff = _aligned_malloc(size, align);
    if (!buff) {
        throw std::runtime_error("failed to allocate buffer.");
    }
}

Buffer::~Buffer()
{
    _aligned_free(buff);
    buff = nullptr;
}

void* Buffer::data()
{
    return buff;
}

