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


#include <sys/timeb.h>
#include <ctime>
#include <io.h>
#include <fcntl.h>
#include <cstdio>
#include <cinttypes>

#include "avs2pipemod.h"
#include "utils.h"
#include "wave.h"


static const AVS_Linkage* AVS_linkage = nullptr;

static inline int64_t get_current_time(void)
{
    timeb tb;
    ftime(&tb);
    return static_cast<int64_t>(tb.time) * 1000 + static_cast<int64_t>(tb.millitm) * 1000;
}



Avs2PipeMod::Avs2PipeMod(HMODULE d, ise_t* e, PClip c, const char* in) :
    dll(d), env(e), clip(c), input(in), versionString("unknown")
{
    vi = clip->GetVideoInfo();

    auto v = env->Invoke("VersionNumber", AVSValue(nullptr, 0));
    validate(!v.IsFloat(), "VersionNumber did not return a float value.\n");
    version = static_cast<float>(v.AsFloat());

    v = env->Invoke("VersionString", AVSValue(nullptr, 0));
    validate(!v.IsString(), "VersionString did not return string.\n");
    versionString = v.AsString();

    sampleSize = vi.BytesFromPixels(1);
    if (vi.IsRGB32()) sampleSize /= 4;
    if (vi.IsRGB24()) sampleSize /= 3;
    if (vi.IsYUY2()) sampleSize /= 2;
}


Avs2PipeMod::~Avs2PipeMod()
{
    clip.~PClip();
    AVS_linkage = nullptr;
    env->DeleteScriptEnvironment();
    FreeLibrary(dll);
}


void Avs2PipeMod::
invokeFilter(const char* filter, AVSValue args, const char** names)
{
    a2pm_log(LOG_INFO, "invoking %s ...\n", filter);
    try {
        clip = env->Invoke(filter, args, names).AsClip();
        vi = clip->GetVideoInfo();
    } catch (AvisynthError& e) {
        throw std::runtime_error(e.msg);
    }
}


void Avs2PipeMod::trim(int* args)
{
    if (args[0] == 0 && args[1] == 0) {
        return;
    }

    AVSValue array[] = { clip, args[0], args[1] };
    invokeFilter("Trim", AVSValue(array, 3));
}



void Avs2PipeMod::info(bool act_info)
{
    printf("\navisynth_version %.3f / %s\n", version, versionString);
    printf("script_name      %s\n\n", input);

    if (vi.HasVideo()) {
        printf("v:width          %d\n", vi.width);
        printf("v:height         %d\n", vi.height);
        printf("v:image_type     %s\n",
               vi.IsFieldBased() ? "fieldbased" : "framebased");
        printf("v:field_order    %s\n",
               vi.IsBFF() ? "assumed bottom field first" :
               vi.IsTFF() ? "assumed top field first" : "not specified");
        printf("v:pixel_type     %s\n", get_string_info(vi.pixel_type));
        printf("v:bit_depth      %d\n", sampleSize * 8);
        printf("v:fps            %u/%u\n", vi.fps_numerator, vi.fps_denominator);
        printf("v:frames         %d\n", vi.num_frames);
        printf("v:duration[sec]  %.3f\n\n",
               1.0 * vi.num_frames * vi.fps_denominator / vi.fps_numerator);
    }

    if (vi.HasAudio() && act_info) {
        printf("a:sample_rate    %d\n", vi.audio_samples_per_second);
        printf("a:format         %s\n",
               vi.sample_type == SAMPLE_FLOAT ? "float" : "integer");
        printf("a:bit_depth      %d\n", vi.BytesPerChannelSample() * 8);
        printf("a:channels       %d\n", vi.AudioChannels());
        printf("a:samples        %" PRIi64 "\n", vi.num_audio_samples);
        printf("a:duration[sec]  %.3f\n\n",
               1.0 * vi.num_audio_samples / vi.audio_samples_per_second);
    }
}


void Avs2PipeMod::benchmark(Params& params)
{
    constexpr int FRAMES_PER_OUT = 50;
    validate(!vi.HasVideo(), "clip has no video.\n");
    trim(params.trim);
    info(false);

    int surplus = vi.num_frames % FRAMES_PER_OUT;
    int passed = 0;

    a2pm_log(LOG_INFO, "benchmarking %d frames video.\n", vi.num_frames);

    int64_t start = get_current_time();
    int64_t elapsed;

    while (passed < surplus) {
        clip->GetFrame(passed++, env);
    }

    while (passed < vi.num_frames) {
        elapsed = get_current_time() - start;
        double de = elapsed * 0.000001;
        fprintf(stderr, "\r"
                "avs2pipemod[info]: [elapsed %.3f sec] %d/%d frames "
                "[%3d%%][%.3ffps]",
                de, passed, vi.num_frames, passed * 100 / vi.num_frames,
                passed / de);
        for (int n = 0; n < FRAMES_PER_OUT; ++n) {
            clip->GetFrame(passed++, env);
        }
    }

    elapsed = get_current_time() - start;

    fprintf(stderr, "\n");
    printf("benchmark result: total elapsed time is %.3f sec [%.3ffps]\n",
            elapsed / 1000000.0, passed * 1000000.0 / elapsed);
    fflush(stdout);

    validate(passed != vi.num_frames,
             "only passed %d of %d frames.\n", passed, vi.num_frames);
}


static void write_audio_file_header(Params& pr, const VideoInfo& vi)
{
    WaveFormatType format = vi.sample_type == SAMPLE_FLOAT ?
        WAVE_FORMAT_IEEE_FLOAT : WAVE_FORMAT_PCM;
    wave_args_t args = {format, vi.nchannels, vi.audio_samples_per_second,
        vi.BytesPerChannelSample(), vi.num_audio_samples};

    if (pr.format_type == FMT_WAVEFORMATEXTENSIBLE) {
        auto header = WaveRiffExtHeader(args);
        fwrite(&header, sizeof(header), 1, stdout);
        return;
    }
    if (pr.format_type == FMT_WAVEFORMATEX) {
        auto header = WaveRiffHeader(args);
        fwrite(&header, sizeof(header), 1, stdout);
    }
}


void Avs2PipeMod::outAudio(Params& params)
{
    validate(!vi.HasAudio(), "clip has no audio.\n");
    trim(params.trim);

    validate(_setmode(_fileno(stdout), _O_BINARY) == -1,
             "cannot switch stdout to binary mode.\n");

    if (params.bit) {
        auto filter = (std::string("ConvertAudioTo") + params.bit).c_str();
        invokeFilter(filter, clip);
    }

    size_t step = 0;
    size_t count = vi.audio_samples_per_second;
    size_t size = vi.BytesPerChannelSample() * vi.nchannels;
    uint64_t target = vi.num_audio_samples;
    auto buff = Buffer(size * count);
    void* data = buff.data();

    a2pm_log(LOG_INFO, "writing %.3f seconds of %zu Hz, %zu channel audio.\n",
             1.0 * target / count, count, vi.nchannels);

    int64_t elapsed = get_current_time();

    if (params.format_type != FMT_RAWAUDIO) {
        write_audio_file_header(params, vi);
    }

    clip->GetAudio(data, 0, target % count, env);
    uint64_t wrote = fwrite(data, size, target % count, stdout);

    while (wrote < target) {
        clip->GetAudio(data, wrote, count, env);
        step = fwrite(data, size, count, stdout);
        if (step != count) break;
        wrote += step;
        a2pm_log(LOG_REPEAT, "wrote %.3f seconds [%" PRIu64 "%%]",
                 1.0 * wrote / count, (100 * wrote) / target);
    }

    fflush(stdout);

    elapsed = get_current_time() - elapsed;

    a2pm_log(LOG_INFO, "total elapsed time is %.3f sec.\n",
             elapsed / 1000000.0);

    validate(wrote != target, "only wrote %I64u of %I64u samples.\n",
             wrote, target);
}


void Avs2PipeMod::prepareY4MOut(Params& params)
{
    if (vi.IsFieldBased()) {
        a2pm_log(LOG_WARNING,
                 "clip is FieldBased.\n"
                 "%19s yuv4mpeg2's spec doesn't support fieldbased clip.\n"
                 "%19s choose what you want.\n"
                 "%19s 1:add AssumeFrameBased()  2:add Weave()  others:exit\n"
                 "%19s input a number and press enter : ",
                 "", "", "", "");
        int k = fgetc(stdin);
        validate(k != '1' && k != '2', "good bye...\n");
        invokeFilter(k == '1' ? "AssumeFrameBased" : "Weave", clip);
    }

    if (vi.IsYUY2()) {
        invokeFilter("ConvertToYV16", clip);
    }
    if (vi.IsRGB()) {
        const char* names[] = {nullptr, "interlaced", "matrix"};
        AVSValue args[] = {
            clip,
            params.frame_type != 'p',
            vi.height < 720 ? "Rec601" : "Rec709"
        };
        invokeFilter("ConvertToYV24", AVSValue(args, 3), names);
    }
    if (sampleSize == 4) {
        invokeFilter("ConvertTo16bit", clip);
    }
}


template <bool Y4MOUT>
int Avs2PipeMod::writeFrames(Params& params)
{
    const int planes[] = { 0, PLANAR_U, PLANAR_V };
    const int num_planes = (vi.pixel_type & VideoInfo::CS_INTERLEAVED) ? 1 : 3;
    const size_t buffsize = vi.width * vi.height * vi.BytesFromPixels(1);
    auto b = Buffer(buffsize, 64);
    uint8_t* buff = reinterpret_cast<uint8_t*>(b.data());

    if (Y4MOUT) {
      fprintf(stdout, "YUV4MPEG2 W%d H%d F%u:%u I%c A%d:%d C%s\n",
        vi.width, vi.height, vi.fps_numerator, vi.fps_denominator,
        params.frame_type, params.sar[0], params.sar[1],
        get_string_y4mheader(vi.pixel_type, params.yuv_depth));
    }

    int wrote = 0;
    while (wrote < vi.num_frames) {
        auto frame = clip->GetFrame(wrote, env);
        if (Y4MOUT) {
            puts("FRAME");
        }
        for (int p = 0; p < num_planes; ++p) {
            int plane = planes[p];
            const uint8_t* srcp = frame->GetReadPtr(plane);
            int rowsize = frame->GetRowSize(plane);
            int pitch = frame->GetPitch(plane);
            int height = frame->GetHeight(plane);
            size_t count = rowsize * height;
            size_t step = 0;

            if (rowsize == pitch) {
                step = fwrite(srcp, 1, count, stdout);
            } else {
                env->BitBlt(buff, rowsize, srcp, pitch, rowsize, height);
                step = fwrite(buff, 1, count, stdout);
            }
            if (step != count) {
                goto finish;
            }
        }
        ++wrote;
    }

finish:
    fflush(stdout);
    return wrote;
}


void Avs2PipeMod::outVideo(Params& params)
{
    validate(!vi.HasVideo(), "clip has no video.\n");
    trim(params.trim);

    validate(_setmode(_fileno(stdout), _O_BINARY) == -1,
        "cannot switch stdout to binary mode.\n");

    if (params.format_type == FMT_RAWVIDEO_VFLIP) {
        invokeFilter("FlipVertical", clip);
    }

    bool y4mout = params.format_type == FMT_YUV4MPEG2;
    char msg[256];
    if (y4mout) {
        prepareY4MOut(params);
        const char* type = params.frame_type == 'p' ? "progressive" :
                           params.frame_type == 't' ? "tff" : "bff";
        snprintf(msg, 256,
                 "writing %d frames of %d/%d fps, %dx%d,\n"
                 "%18s sar %d:%d, %s %s video.\n",
                 vi.num_frames, vi.fps_numerator, vi.fps_denominator, vi.width,
                 vi.height, "", params.sar[0], params.sar[1],
                 get_string_video_out(vi.pixel_type), type);
    } else {
        snprintf(msg, 256,
                 "writing %d frames of %dx%d %s rawvideo.\n",
                 vi.num_frames, vi.width, vi.height,
                 get_string_video_out(vi.pixel_type));
    }
    a2pm_log(LOG_INFO, msg);

    int64_t elapsed = get_current_time();

    int wrote = y4mout ? writeFrames<true>(params) : writeFrames<false>(params);

    elapsed = get_current_time() - elapsed;

    a2pm_log(LOG_INFO, "finished, wrote %d frames [%d%%].\n",
             wrote, 100 * wrote / vi.num_frames);
    a2pm_log(LOG_INFO, "total elapsed time is %.3f sec.\n",
             elapsed / 1000000.0);

    validate(wrote != vi.num_frames, "only wrote %d of %d frames.\n",
             wrote, vi.num_frames);
}


template <typename T, bool IS_INTERLEAVED>
int Avs2PipeMod::writePixValuesAsText()
{
    const int planes[] = { 0, PLANAR_U, PLANAR_V };
    constexpr int NUM_PLANES = IS_INTERLEAVED ? 1 : 3;

    int wrote = 0;
    while (wrote < vi.num_frames) {
        auto frame = clip->GetFrame(wrote, env);
        fprintf(stdout, "frame %d\n", wrote);

        for (int p = 0; p < NUM_PLANES; ++p) {
            int plane = planes[p];
            const uint8_t* srcp = frame->GetReadPtr(plane);
            int width = frame->GetRowSize(plane) / sizeof(T);
            int height = frame->GetHeight(plane);
            int pitch = frame->GetPitch(plane);

            if (!IS_INTERLEAVED) {
                puts(p == 0 ? "Y-plane" : p == 1 ? "U-plane" : "V-plane");
            }

            for (int y = 0; y < height; ++y) {
                const T* s0 = reinterpret_cast<const T*>(srcp);
                for (int x = 0; x < width; ++x) {
                    if (sizeof(T) != 4) {
                        fprintf(stdout, "%u\t", static_cast<unsigned>(s0[x]));
                    } else {
                        fprintf(stdout, "%.8f\t", static_cast<float>(s0[x]));
                    }
                }
                fputc('\n', stdout);
                srcp += pitch;
            }
            fputc('\n', stdout);
        }
        ++wrote;
    }
    fflush(stdout);
    return wrote;
}


void Avs2PipeMod::dumpPixValues(Params& params)
{
    validate(!vi.HasVideo(), "clip has no video.\n");
    trim(params.trim);

    a2pm_log(LOG_INFO,
             "writing pixel values of %dx%dx%dframes to stdout as text.\n",
             vi.width, vi.height, vi.num_frames);

    info(false);
    puts("\n");

    int wrote = 0;
    if (vi.IsRGB() || vi.IsYUY2() || vi.IsY8()) {
        wrote = writePixValuesAsText<uint8_t, true>();
    } else if (vi.IsColorSpace(VideoInfo::CS_Y16)) {
        wrote = writePixValuesAsText<uint16_t, true>();
    } else if (vi.IsColorSpace(VideoInfo::CS_Y32)) {
        wrote = writePixValuesAsText<float, true>();
    } else if (sampleSize == 1) {
        wrote = writePixValuesAsText<uint8_t, false>();
    } else if (sampleSize == 2 ) {
        wrote = writePixValuesAsText<uint16_t, false>();
    } else {
        wrote = writePixValuesAsText<float, false>();
    }

    a2pm_log(LOG_INFO, "finished, wrote %d frames [%d%%].\n",
             wrote, 100 * wrote / vi.num_frames);

    validate(wrote != vi.num_frames, "only wrote %d of %d frames.\n",
             wrote, vi.num_frames);
}


Avs2PipeMod* Avs2PipeMod::create(const char* input, const char* dll_path)
{
    typedef ise_t* (__stdcall *cse_t)(int);
    constexpr int AVS_INTERFACE_VERSION = 6;

    HMODULE dll = nullptr;
    ise_t* env = nullptr;

    try {
        dll = LoadLibraryExA(dll_path ? dll_path : "avisynth", nullptr,
                             LOAD_WITH_ALTERED_SEARCH_PATH);
        validate(!dll, "failed to load avisynth.dll\n");

        cse_t create_env = reinterpret_cast<cse_t>(
            GetProcAddress(dll, "CreateScriptEnvironment"));
        validate(!create_env, "failed to get CreateScriptEnvironment().\n");

        env = create_env(AVS_INTERFACE_VERSION);
        validate(!env, "failed to create avisynth script environment.\n");

        AVS_linkage = env->GetAVSLinkage();

        AVSValue res = env->Invoke("Import", AVSValue(input));
        validate(!res.IsClip(), "Script didn't return a clip.\n");

        return new Avs2PipeMod(dll, env, res.AsClip(), input);

    } catch (std::runtime_error& e) {
        AVS_linkage = nullptr;
        if (env) {
            env->DeleteScriptEnvironment();
        }
        if (dll) {
            FreeLibrary(dll);
        }
        throw e;
    } catch (AvisynthError e) {
        fprintf(stderr, "avs2pipemod[error]: %s\n", e.msg);
        AVS_linkage = nullptr;
        env->DeleteScriptEnvironment();
        FreeLibrary(dll);
        exit(-1);
    } catch (...) {
        throw std::runtime_error("unkown exception.\n");
    }

    return nullptr;
}

