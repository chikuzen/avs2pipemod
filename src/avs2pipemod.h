/*
 * Copyright (C) 2012-2016 Oka Motofumi <chikuzen.mo at gmail dot com>
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

#ifndef AVS2PIPEMOD_H
#define AVS2PIPEMOD_H


#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define NOGDI
#include <windows.h>
#include <avisynth.h>

#define A2PM_VERSION "1.1.0"


enum action_t {
    A2PM_ACT_INFO,
    A2PM_ACT_BENCHMARK,
    A2PM_ACT_AUDIO,
    A2PM_ACT_VIDEO,
    A2PM_ACT_DUMP_PIXEL_VALUES_AS_TXT,
    A2PM_ACT_FILTERS,
#if 0
    A2PM_ACT_X264BD,
    A2PM_ACT_X264RAW,
#endif
    A2PM_ACT_NOTHING
};

enum format_type_t{
    FMT_RAWVIDEO,
    FMT_RAWVIDEO_VFLIP,
    FMT_YUV4MPEG2,
    FMT_RAWAUDIO,
    FMT_WAVEFORMATEX,
    FMT_WAVEFORMATEXTENSIBLE,
#if 0
    FMT_HDBD,
    FMT_SDBD,
    FMT_WITHOUT_TCFILE,
    FMT_WITH_TCFILE,
#endif
    FMT_NOTHING
};


struct Params {
    action_t action;
    format_type_t format_type;
    int sar[2];
    int trim[2];
    char frame_type;
    char* bit;
    int yuv_depth;
    char* dll_path;
};


typedef IScriptEnvironment ise_t;

class Avs2PipeMod {
    HMODULE dll;
    ise_t* env;
    PClip clip;
    VideoInfo vi;
    float version;
    const char* versionString;
    const char* input;
    int sampleBits;
    int numPlanes;

    void invokeFilter(const char* filter, AVSValue args, const char** names=nullptr);
    void trim(int* args);
    void prepareY4MOut(Params& params);
    template <bool y4mout> int writeFrames(Params& params);
    template <typename T> int writePixValuesAsText();

public:
    Avs2PipeMod(HMODULE dll, ise_t* env, PClip clip, const char* input);
    ~Avs2PipeMod();
    void info(bool act_info);
    void benchmark(Params& params);
    void outAudio(Params& params);
    void outVideo(Params& params);
    void dumpPixValues(Params& params);
    void dumpPluginFiltersList(Params& params);
#if 0
    void x264bd(Params& params);
    void x264raw(Params& params);
#endif
    static Avs2PipeMod* create(const char* input, const char* dll_path);
};



#endif /* AVS2PIPEMOD_H */
