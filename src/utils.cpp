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

    va_start(args, message);
    vfprintf(stderr, message, args);
    va_end(args);
}


const char* get_string_info(int pix_type)
{
    switch (pix_type) {
    case VideoInfo::CS_BGR32:     return "RGB32";
    case VideoInfo::CS_BGR24:     return "RGB24";
    case VideoInfo::CS_YUY2:      return "YUY2";
    case VideoInfo::CS_YV24:      return "YV24";
    case VideoInfo::CS_YV16:      return "YV16";
    case VideoInfo::CS_YV12:
    case VideoInfo::CS_I420:      return "YV12";
    case VideoInfo::CS_Y8:        return "Y8";
    case VideoInfo::CS_YUV444PS:  return "YUV444PS";
    case VideoInfo::CS_YUV444P16: return "YUV444P16";
    case VideoInfo::CS_YUV422PS:  return "YUV422PS";
    case VideoInfo::CS_YUV422P16: return "YUV422P16";
    case VideoInfo::CS_YUV420PS:  return "YUV420PS";
    case VideoInfo::CS_YUV420P16: return "YUV420P16";
    case VideoInfo::CS_Y32:       return "YUV420PS";
    case VideoInfo::CS_Y16:       return "YUV420P16";
    default: return "unknown";
    }
}


const char* get_string_video_out(int pix_type)
{
    switch (pix_type) {
    case VideoInfo::CS_BGR32:     return "BGRA";
    case VideoInfo::CS_BGR24:     return "BGR";
    case VideoInfo::CS_YUY2:      return "YUYV-422-packed-8bit";
    case VideoInfo::CS_YV24:      return "YUV-444-planar-8bit";
    case VideoInfo::CS_YV16:      return "YUV-422-planar-8bit";
    case VideoInfo::CS_YV12:
    case VideoInfo::CS_I420:      return "YUV-420-planar-8bit";
    case VideoInfo::CS_Y8:        return "luma-only-8bit";
    case VideoInfo::CS_YUV444PS:  return "YUV-444-planar-float";
    case VideoInfo::CS_YUV444P16: return "YUV-444-planar-16bit";
    case VideoInfo::CS_YUV422PS:  return "YUV-422-planar-float";
    case VideoInfo::CS_YUV422P16: return "YUV-422-planar-16bit";
    case VideoInfo::CS_YUV420PS:  return "YUV-420-planar-float";
    case VideoInfo::CS_YUV420P16: return "YUV-420-planar-16bit";
    case VideoInfo::CS_Y32:       return "luma-only-float";
    case VideoInfo::CS_Y16:       return "luma-only-16bit";
    default: return "unknown";
    }
}


const char* get_string_y4mheader(int pix_type, int bits=16)
{
    switch (pix_type) {
    case VideoInfo::CS_YV24:  return "444";
    case VideoInfo::CS_YV16:  return "422";
    case VideoInfo::CS_YV12:
    case VideoInfo::CS_I420:  return "420mpeg2";
    case VideoInfo::CS_YV411: return "411";
    case VideoInfo::CS_Y8:    return "mono";
    case VideoInfo::CS_YUV444P16:
        switch (bits) {
        case  9: return "444p9";
        case 10: return "444p10";
        case 12: return "444p12";
        case 14: return "444p14";
        default: return "444p16";
        }
    case VideoInfo::CS_YUV422P16:
        switch (bits) {
        case  9: return "422p9";
        case 10: return "422p10";
        case 12: return "422p12";
        case 14: return "422p14";
        default: return "422p16";
        }
    case VideoInfo::CS_YUV420P16:
        switch (bits) {
        case  9: return "420p9";
        case 10: return "420p10";
        case 12: return "420p12";
        case 14: return "420p14";
        default: return "420p16";
        }
    case VideoInfo::CS_Y16: return "mono16";
    default: return nullptr;
    }
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

