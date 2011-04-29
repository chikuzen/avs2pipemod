/*
 * Copyright (C) 2010-2011 Chris Beswick <chris.beswick@gmail.com>
 *
 * YUV4MPEG2 output inspired by Avs2YUV by Loren Merritt
 *
 * This file is part of avs2pipe.
 *
 * avs2pipe is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * avs2pipe is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with avs2pipe.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <fcntl.h>
#include <io.h>
#include <string.h>

#include "common.h"
#include "wave.h"

#ifdef A2P_AVS26
    #include "avisynth26/avisynth_c.h"
#else
    #include "avisynth25/avisynth_c.h"
#endif

#include "version.h"

#define BM_FRAMES_PAR_OUT 50
#define Y4M_FRAME_HEADER_SIZE 6
#define BUFSIZE_OF_STDOUT 262144 //The optimum value might be smaller than this.

AVS_Clip *
avisynth_filter(AVS_Clip *clip, AVS_ScriptEnvironment *env, const char *filter)
{
    AVS_Value val_clip, val_array, val_return;

    val_clip = avs_new_value_clip(clip);
    avs_release_clip(clip);

    val_array = avs_new_value_array(&val_clip, 1);
    val_return = avs_invoke(env, filter, val_array, 0);
    clip = avs_take_clip(val_return, env);

    avs_release_value(val_array);
    avs_release_value(val_clip);
    avs_release_value(val_return);

    return clip;
}

AVS_Clip *
avisynth_source(char *file, AVS_ScriptEnvironment *env)
{
    AVS_Clip *clip;
    AVS_Value val_string, val_array, val_return;

    val_string = avs_new_value_string(file);
    val_array = avs_new_value_array(&val_string, 1);
    val_return = avs_invoke(env, "Import", val_array, 0);
    avs_release_value(val_array);
    avs_release_value(val_string);

    if(!avs_is_clip(val_return)) {
        a2p_log(A2P_LOG_ERROR, "avs files return value is not a clip.\n", 0);
    }

    clip = avs_take_clip(val_return, env);

    avs_release_value(val_return);

    return clip;
}

void
do_audio(AVS_Clip *clip, AVS_ScriptEnvironment *env, int bit)
{
    const AVS_VideoInfo *info;
    WaveRiffHeader *header;
    WaveFormatType format;
    void *buff;
    size_t size, count, step;
    uint64_t i, wrote, target;
    int64_t start, end, elapsed;

    info = avs_get_video_info(clip);

    if(!avs_has_audio(info))
        a2p_log(A2P_LOG_ERROR, "clip has no audio.\n", 0);

    if(bit) {
        a2p_log(A2P_LOG_INFO, "converting bit depth of audio to %dbit integer.\n", bit);
        clip = (bit == 16) ? avisynth_filter(clip, env, "ConvertAudioTo16bit") :
                             avisynth_filter(clip, env, "ConvertAudioTo24bit");
        info = avs_get_video_info(clip);
    }

    if(info->sample_type == AVS_SAMPLE_FLOAT)
        format = WAVE_FORMAT_IEEE_FLOAT;
    else
        format = WAVE_FORMAT_PCM;

    if(_setmode(_fileno(stdout), _O_BINARY) == -1)
        a2p_log(A2P_LOG_ERROR, "cannot switch stdout to binary mode.\n", 0);

    a2p_log(A2P_LOG_INFO, "writing %.3f seconds of %d Hz, %d channel audio.\n",
            (double)info->num_audio_samples / info->audio_samples_per_second,
            info->audio_samples_per_second, info->nchannels);

    start = a2pm_gettime();

    header = wave_create_riff_header(format, info->nchannels,
                                     info->audio_samples_per_second,
                                     avs_bytes_per_channel_sample(info),
                                     info->num_audio_samples);

    fwrite(header, sizeof(*header), 1, stdout);

    count = info->audio_samples_per_second;
    wrote = 0;
    target = info->num_audio_samples;
    size = avs_bytes_per_channel_sample(info) * info->nchannels;
    buff = malloc(count * size);

    for (i = 0; i < target; i += count) {
        if(target - i < count)
            count = (size_t) (target - i);

        avs_get_audio(clip, buff, i, count);

        step = fwrite(buff, size, count, stdout);

        // fail early if there is a problem instead of end of input
        if(step != count)
            break;

        wrote += count;
        //a2p_log(A2P_LOG_REPEAT, "written %lld seconds [%lld%%]... ",
        //    wrote / info->audio_samples_per_second,
        //    (100 * wrote) / target);
    }

    fflush(stdout); // clear buffers before we exit

    a2p_log(A2P_LOG_REPEAT, "finished, wrote %.3f seconds [%I64u%%].\n",
            (double)wrote / info->audio_samples_per_second, (100 * wrote) / target);

    free(buff);
    free(header);

    end = a2pm_gettime();
    elapsed = end - start;
    a2p_log(A2P_LOG_INFO, "total elapsed time is %.3f sec.\n", elapsed / 1000000.0);

    if(wrote != target)
        a2p_log(A2P_LOG_ERROR, "only wrote %I64u of %I64u samples.\n", wrote, target);
}

void
do_y4m(AVS_Clip *clip, AVS_ScriptEnvironment *env, char ip, int sar_x, int sar_y)
{
    const AVS_VideoInfo *info;
    AVS_VideoFrame *frame;
    static const int planes[] = {AVS_PLANAR_Y, AVS_PLANAR_U, AVS_PLANAR_V};
    const BYTE *buff; // BYTE from avisynth_c.h not windows headers
    char *yuv_csp = NULL;
    size_t count, step;
    int32_t f, p, i, h_uv, v_uv, w, h, pitch, np, k;
    int32_t wrote, target;
    int64_t start, end, elapsed;

    info = avs_get_video_info(clip);

    if(!avs_has_video(info))
        a2p_log(A2P_LOG_ERROR, "clip has no video.\n");

    if(avs_is_field_based(info)) {
        a2p_log(A2P_LOG_WARNING, "clip is FieldBased.\n");
        fprintf(stderr, "           yuv4mpeg2's spec doesn't support FieldBased clip.\n"
                        "           choose what you want.\n"
                        "           1:add AssumeFrameBased()  2:add Weave()  others:exit\n"
                        "               input a number and press enter : ");
        fscanf(stdin, "%d", &k);
        switch(k) {
            case 1:
                a2p_log(A2P_LOG_INFO, "clip was assumed that FrameBased.\n", 0);
                clip = avisynth_filter(clip, env, "AssumeFrameBased");
                info = avs_get_video_info(clip);
                break;
            case 2:
                a2p_log(A2P_LOG_INFO, "clip was Weaved\n", 0);
                clip = avisynth_filter(clip, env, "Weave");
                info = avs_get_video_info(clip);
                break;
            default:
                a2p_log(A2P_LOG_ERROR, "good bye...\n", 2);
        }
    }

    // Number of planes normally 3;
    np = 3;

    // Setup Correct Colorspace Handling
    // Thanks to Chikuzen @ Doom9 Forums
    #ifdef A2P_AVS26
    switch(info->pixel_type) {
        case AVS_CS_BGR32:
        case AVS_CS_BGR24:
            a2p_log(A2P_LOG_INFO, "converting video to planar yuv444.\n", 0);
            clip = avisynth_filter(clip, env, "ConvertToYV24");
            info = avs_get_video_info(clip);
        case AVS_CS_YV24:
            yuv_csp = "444";
            h_uv = 0;
            v_uv = 0;
            break;
        case AVS_CS_YUY2:
            a2p_log(A2P_LOG_INFO, "converting video to planar yuv422.\n", 0);
            clip = avisynth_filter(clip, env, "ConvertToYV16");
            info = avs_get_video_info(clip);
        case AVS_CS_YV16:
            yuv_csp = "422";
            h_uv = 1;
            v_uv = 0;
            break;
        case AVS_CS_YV411:
            yuv_csp = "411";
            h_uv = 2;
            v_uv = 0;
            break;
        case AVS_CS_Y8:
            yuv_csp = "mono";
            h_uv = 0;
            v_uv = 0;
            np = 1; // special case only one plane for mono
            break;
        default:
    #endif
            if((info->pixel_type != AVS_CS_I420) && (info->pixel_type != AVS_CS_YV12)) {
                a2p_log(A2P_LOG_INFO, "converting video to planar yuv420.\n", 0);
                if(!(info->width % 2) && !(info->height % 2)) {
                    clip = avisynth_filter(clip, env, "ConvertToYV12");
                    info = avs_get_video_info(clip);
                } else
                    a2p_log(A2P_LOG_ERROR, "invalid resolution. conversion failed.\n", 0);
            }
            yuv_csp = "420mpeg2"; //same as avisynth default subsampling type of yuv420
            h_uv = 1;
            v_uv = 1;
    #ifdef A2P_AVS26
    }
    #endif

    if(!avs_is_planar(info))
        a2p_log(A2P_LOG_ERROR, "colorspace handling failed. leaving...\n", 2);

    count = (info->width * info-> height * avs_bits_per_pixel(info)) >> 3;

    if(_setmode(_fileno(stdout), _O_BINARY) == -1)
        a2p_log(A2P_LOG_ERROR, "cannot switch stdout to binary mode.\n");

    a2p_log(A2P_LOG_INFO, "writing %d frames of %d/%d fps, %dx%d, sar %d:%d YUV%s %s video.\n",
            info->num_frames, info->fps_numerator, info->fps_denominator,
            info->width, info->height, sar_x, sar_y, yuv_csp,
            ip == 'p' ? "progressive" : ip == 't' ? "tff" : "bff");

    start = a2pm_gettime();

    // YUV4MPEG2 header http://linux.die.net/man/5/yuv4mpeg
    fprintf(stdout, "YUV4MPEG2 W%d H%d F%u:%u I%c A%d:%d C%s\n",
            info->width, info->height, info->fps_numerator,
            info->fps_denominator, ip, sar_x, sar_y, yuv_csp);
    fflush(stdout);

    // method from avs2yuv converted to c
    setvbuf(stdout, NULL, _IOFBF, BUFSIZE_OF_STDOUT);

    target = info->num_frames;
    wrote = 0;

    for(f = 0; f < target; f++) {
        step = 0;
        frame = avs_get_frame(clip, f);
        fprintf(stdout, "FRAME\n");
        for(p = 0; p < np; p++) {
            w = info->width >> (p ? h_uv : 0);
            h = info->height >> (p ? v_uv : 0);
            pitch = avs_get_pitch_p(frame, planes[p]);
            buff = avs_get_read_ptr_p(frame, planes[p]);
            for(i = 0; i < h; i++) {
                step += fwrite(buff, sizeof(BYTE), w, stdout);
                buff += pitch;
            }
        }
        avs_release_frame(frame);

        // fail early if there is a problem instead of end of input
        if(step != count)
            break;

        wrote++;
        //a2p_log(A2P_LOG_REPEAT, "written %d frames [%d%%]... ",
        //    wrote, (100 * wrote) / target);
    }

    fflush(stdout); // clear buffers before we exit

    end = a2pm_gettime();
    elapsed = end - start;

    a2p_log(A2P_LOG_REPEAT, "finished, wrote %d frames [%d%%].\n",
        wrote, (100 * wrote) / target);

    a2p_log(A2P_LOG_INFO, "total elapsed time is %.3f sec [%.3ffps].\n",
        elapsed / 1000000.0, wrote * 1000000.0 / elapsed);

    if(wrote != target)
        a2p_log(A2P_LOG_ERROR, "only wrote %d of %d frames.\n", wrote, target);
}

void
do_packedraw(AVS_Clip *clip, AVS_ScriptEnvironment *env)
{
    const AVS_VideoInfo *info;
    AVS_VideoFrame *frame;
    const BYTE *buff; // BYTE from avisynth_c.h not windows headers
    char *pix_fmt;
    size_t count, step;
    int32_t pitch, f, w, h, i;
    int32_t wrote, target;
    int64_t start, end, elapsed;

    info = avs_get_video_info(clip);

    if(!avs_has_video(info))
        a2p_log(A2P_LOG_ERROR, "clip has no video.\n");

    switch(info->pixel_type) {
        case AVS_CS_BGR32:
            pix_fmt = "BGRA";
            break;
        case AVS_CS_BGR24:
            pix_fmt = "BGR";
            break;
        case AVS_CS_YUY2:
            pix_fmt = "YUYV";
            break;
        #ifdef A2P_AVS26
        case AVS_CS_Y8:
            pix_fmt = "Y8(gray)";
            break;
        #endif
        default:
            a2p_log(A2P_LOG_ERROR, "invalid pixel_type in this mode.\n", 0);
    }

    w = info->width * avs_bits_per_pixel(info) >> 3;
    h = info->height;
    count = w * h;

    if(_setmode(_fileno(stdout), _O_BINARY) == -1)
        a2p_log(A2P_LOG_ERROR, "cannot switch stdout to binary mode.\n");

    a2p_log(A2P_LOG_INFO, "writing %d frames of %dx%d %s rawvideo.\n",
            info->num_frames, info->width, info->height, pix_fmt);

    target = info->num_frames;
    wrote = 0;

    setvbuf(stdout, NULL, _IOFBF, BUFSIZE_OF_STDOUT);

    start = a2pm_gettime();

    for(f = 0; f < target; f++) {
        step = 0;
        frame = avs_get_frame(clip, f);
        pitch = avs_get_pitch(frame);
        buff = avs_get_read_ptr(frame);
        for(i = 0; i < h; i++) {
            step += fwrite(buff, sizeof(BYTE), w, stdout);
            buff += pitch;
        }

        avs_release_frame(frame);

        if(step != count)
            break;

        wrote++;
    }

    end = a2pm_gettime();
    elapsed = end - start;

    a2p_log(A2P_LOG_REPEAT, "finished, wrote %d frames [%d%%].\n",
        wrote, (100 * wrote) / target);

    a2p_log(A2P_LOG_INFO, "total elapsed time is %.3f sec [%.3ffps].\n",
        elapsed / 1000000.0, wrote * 1000000.0 / elapsed);

    if(wrote != target)
        a2p_log(A2P_LOG_ERROR, "only wrote %d of %d frames.\n", wrote, target);
}

void
do_info(AVS_Clip *clip, AVS_ScriptEnvironment *env, char *input)
{
    const AVS_VideoInfo *info;
    char *field_order, *pix_fmt;

    info = avs_get_video_info(clip);

    if(avs_has_video(info)) {
        fprintf(stdout, "script_name      %s\n", input);
        fprintf(stdout, "v:width          %d\n", info->width);
        fprintf(stdout, "v:height         %d\n", info->height);
        fprintf(stdout, "v:fps            %d/%d\n",
                info->fps_numerator, info->fps_denominator);
        fprintf(stdout, "v:frames         %d\n", info->num_frames);
        fprintf(stdout, "v:duration[sec]  %.3f\n",
                (double)info->num_frames * info->fps_denominator / info->fps_numerator);
        fprintf(stdout, "v:image_type     %s\n",
                !avs_is_field_based(info) ? "framebased" : "fieldbased");
        switch(info->image_type) {
            case 1:
            case 5:
                field_order = "assumed bottom field first";
                break;
            case 2:
            case 6:
                field_order = "assumed top field first";
                break;
            default:
                field_order = "not specified";
        }
        fprintf(stdout, "v:field_order    %s\n", field_order);
        switch(info->pixel_type) {
            #ifdef A2P_AVS26
            case AVS_CS_YV24:
                pix_fmt = "YV24";
                break;
            case AVS_CS_YV16:
                pix_fmt = "YV16";
                break;
            case AVS_CS_YV411:
                pix_fmt = "YV411";
                break;
            case AVS_CS_Y8:
                pix_fmt = "Y8";
                break;
            #endif
            case AVS_CS_BGR32:
                pix_fmt = "RGB32";
                break;
            case AVS_CS_BGR24:
                pix_fmt = "RGB24";
                break;
            case AVS_CS_YUY2:
                pix_fmt = "YUY2";
                break;
            default:
                pix_fmt = "YV12";
        }
        fprintf(stdout, "v:pixel_type     %s\n", pix_fmt);
    }
    if(avs_has_audio(info)) {
        fprintf(stdout, "a:sample_rate    %d\n", info->audio_samples_per_second);
        fprintf(stdout, "a:format         %s\n",
                info->sample_type == AVS_SAMPLE_FLOAT ? "float" : "pcm");
        fprintf(stdout, "a:bit_depth      %d\n",
                avs_bytes_per_channel_sample(info) * 8);
        fprintf(stdout, "a:channels       %d\n", info->nchannels);
        fprintf(stdout, "a:samples        %I64d\n", info->num_audio_samples);
        fprintf(stdout, "a:duration[sec]  %.3f\n",
                (double)info->num_audio_samples / info->audio_samples_per_second);
    }
}

void
do_benchmark(AVS_Clip *clip, AVS_ScriptEnvironment *env, char *input)
{
    const AVS_VideoInfo *info;
    AVS_VideoFrame *frame;
    int32_t f, i;
    int32_t passed, target, count, percentage;
    int64_t start, running, end, elapsed;
    double fps;

    info = avs_get_video_info(clip);

    if(!avs_has_video(info))
        a2p_log(A2P_LOG_ERROR, "clip has no video.\n");

    do_info(clip, env, input);

    target = info->num_frames;
    count = target / BM_FRAMES_PAR_OUT;
    percentage = 0;
    passed = 0;

    a2p_log(A2P_LOG_INFO, "benchmarking %d frames video.\n", target);

    start = a2pm_gettime();

    for(i = 0; i < count; i++) {
        for(f = 0; f < BM_FRAMES_PAR_OUT; f++) {
            frame = avs_get_frame(clip, passed);
            avs_release_frame(frame);
            passed++;
        }
        running = a2pm_gettime();
        elapsed = running - start;
        fps = passed * 1000000.0 / elapsed;
        percentage = passed * 100 / target;
        a2p_log(A2P_LOG_REPEAT, "[elapsed %.3f sec] %d/%d frames [%3d%%][%.3ffps]",
                    elapsed / 1000000.0, passed, target, percentage, fps);
    }

    while(passed < target) {
        frame = avs_get_frame(clip, passed);
        avs_release_frame(frame);
        passed++;
    }

    end = a2pm_gettime();
    elapsed = end - start;
    fps = passed * 1000000.0 / elapsed;

    fprintf(stderr, "\n");
    a2p_log(A2P_LOG_INFO, "%d frames has passed [100%%][%.3ffps]\n", passed, fps);
    fprintf(stdout, "benchmark result: total elapsed time is %.3f sec [%.3ffps]\n",
            elapsed / 1000000.0, fps);

    if(passed != target)
        a2p_log(A2P_LOG_ERROR, "only passed %d of %d frames.\n", passed, target);
}

void
do_x264bd(AVS_Clip *clip, AVS_ScriptEnvironment *env)
{
    // x264 arguments from http://sites.google.com/site/x264bluray/
    // resolutions, fps... http://forum.doom9.org/showthread.php?t=154533
    //   vbv-maxrate reduced to 15000 from 40000 for DVD playback
    //   ref added to all settings for simplicity

    const AVS_VideoInfo *info;
	enum res {
        A2P_RES_1080,
        A2P_RES_720,
        A2P_RES_576,
        A2P_RES_480,
        A2P_RES_UNKNOWN
    } res;
    enum fps {
        A2P_FPS_59, // 59.94
        A2P_FPS_50, // 50
        A2P_FPS_30, // 30
        A2P_FPS_29, // 29.97
        A2P_FPS_25, // 25
        A2P_FPS_24, // 24
        A2P_FPS_23, // 23.976
        A2P_FPS_UNKNOWN
    } fps;

    int keyint;
    int ref;
    char * args;
    char * color;

    info = avs_get_video_info(clip);

    // Initial format guess, setting ref and color.
    // max safe width or height = 32767
    // width << 16         | height
    // 0000 0000 0000 0000   0000 0000  0000 0000
    switch((info->width << 16) | info->height) {
        case (1920 << 16) | 1080: // HD1080
        case (1440 << 16) | 1080:
            res = A2P_RES_1080;
            ref = 4;
            color = "bt709";
            break;
        case (1280 << 16) | 720: // HD720
            res = A2P_RES_720;
            ref = 6;
            color = "bt709";
            break;
        case (720 << 16) | 576: // PAL
            res = A2P_RES_576;
            ref = 6;
            color = "bt470bg";
            break;
        case (720 << 16) | 480: // NTSC
            res = A2P_RES_480;
            ref = 6;
            color = "smpte170m";
            break;
        default:
            res = A2P_RES_UNKNOWN;
            ref = 0;
            color = "";
            break;
    }

    // See http://avisynth.org/mediawiki/FPS
    // fps_numerator << 16 | fps_denominator
    // 0000 0000 0000 0000   0000 0000  0000 0000
    switch((info->fps_numerator << 16) | info->fps_denominator) {
        case ((24000 << 16) | 1001): // ntsc_film ~23.976
        case ((2997 << 16) | 125): // ntsc_round_film =23.976
            fps = A2P_FPS_23;
            keyint = 24;
            break;
        case ((24 << 16) | 1): // film =24
            fps = A2P_FPS_24;
            keyint = 24;
            break;
        case ((25 << 16) | 1): // pal_film, pal_video =25
            fps = A2P_FPS_25;
            keyint = 25;
            break;
        case ((30000 << 16) | 1001): // ntsc_video ~29.970
        case ((2997 << 16) | 100): // ntsc_round_video =29.97
            fps = A2P_FPS_29;
            keyint = 30;
            break;
        case ((30 << 16) | 1): // 1080i30 exists in enc guide =30
            fps = A2P_FPS_30;
            keyint = 30;
            break;
        case ((50 << 16) | 1): // pal_double =50
            fps = A2P_FPS_50;
            keyint = 50;
            break;
        case ((60000 << 16) | 1001): // ntsc_double ~59.940
        case ((2997 << 16) | 50): // ntsc_round_double =59.94
            fps = A2P_FPS_59;
            keyint = 60;
            break;
            break;
        default:
            fps = A2P_FPS_UNKNOWN;
            keyint = 0;
            break;
    }

    // Ensure video is supported... this is messy
    // and set any special case arguments.
    // res << 16           | fps << 8  | avs_is_field_based
    // 0000 0000 0000 0000   0000 0000   0000 0000
    switch((res << 16) | fps << 8 | avs_is_field_based(info)) {
        case ((A2P_RES_1080 << 16) | (A2P_FPS_25 << 8) | 1):
        case ((A2P_RES_1080 << 16) | (A2P_FPS_29 << 8) | 1):
        case ((A2P_RES_1080 << 16) | (A2P_FPS_30 << 8) | 1):
        case ((A2P_RES_576 << 16) | (A2P_FPS_25 << 8) | 1):
        case ((A2P_RES_480 << 16) | (A2P_FPS_29 << 8) | 1):
            args = !avs_is_bff(info) ? "--tff" : "--bff"; // default to tff
            break;
        case ((A2P_RES_1080 << 16) | (A2P_FPS_29 << 8) | 0):
        case ((A2P_RES_1080 << 16) | (A2P_FPS_25 << 8) | 0):
        case ((A2P_RES_576 << 16) | (A2P_FPS_25 << 8) | 0):
            args = "--fake-interlaced --pic-struct";
            break;
        case ((A2P_RES_720 << 16) | (A2P_FPS_29 << 8) | 0):
        case ((A2P_RES_720 << 16) | (A2P_FPS_25 << 8) | 0):
            args = "--pulldown double";
            break;
        case ((A2P_RES_480 << 16) | (A2P_FPS_29 << 8) | 0):
            args = "--pulldown 32 --fake-interlaced";
            break;
        case ((A2P_RES_1080 << 16) | (A2P_FPS_23 << 8) | 0):
        case ((A2P_RES_1080 << 16) | (A2P_FPS_24 << 8) | 0):
        case ((A2P_RES_720 << 16) | (A2P_FPS_23 << 8) | 0):
        case ((A2P_RES_720 << 16) | (A2P_FPS_24 << 8) | 0):
        case ((A2P_RES_720 << 16) | (A2P_FPS_50 << 8) | 0):
        case ((A2P_RES_720 << 16) | (A2P_FPS_59 << 8) | 0):
            args = "";
            break;
        default:
            args = "";
            a2p_log(A2P_LOG_ERROR, "%dx%d @ %d/%d fps not supported.\n",
                    info->width, info->height,
                    info->fps_numerator, info->fps_denominator);
            break;
    }

    fprintf(stdout,
            "--weightp 1 --bframes 3 --nal-hrd vbr --vbv-maxrate 15000"
            " --vbv-bufsize 30000 --level 4.1 --keyint %d --b-pyramid strict"
            " --open-gop bluray --slices 4 --ref %d %s --aud --colorprim "
            "\"%s\" --transfer \"%s\" --colormatrix \"%s\"",
            keyint, ref, args, color, color, color);
}

int __cdecl
main (int argc, char *argv[])
{
    AVS_ScriptEnvironment *env;
    AVS_Clip *clip;
    char *input;
    int sar_x = 0, sar_y = 0;
    enum {
        A2P_ACTION_AUDIO,
        A2P_ACTION_AUD16,
        A2P_ACTION_AUD24,
        A2P_ACTION_Y4MP,
        A2P_ACTION_Y4MT,
        A2P_ACTION_Y4MB,
        A2P_ACTION_PACKEDRAW,
        A2P_ACTION_INFO,
        A2P_ACTION_X264BD,
        A2P_ACTION_BENCHMARK,
        A2P_ACTION_NOTHING
    } action;

    action = A2P_ACTION_NOTHING;

    if(argc == 3 || argc == 4) {
        if(!strcmp(argv[1], "audio"))
            action = A2P_ACTION_AUDIO;
        else if(!strcmp(argv[1], "aud16"))
            action = A2P_ACTION_AUD16;
        else if(!strcmp(argv[1], "aud24"))
            action = A2P_ACTION_AUD24;
        else if(!strcmp(argv[1], "y4mp"))
            action = A2P_ACTION_Y4MP;
        else if(!strcmp(argv[1], "y4mt"))
            action = A2P_ACTION_Y4MT;
        else if(!strcmp(argv[1], "y4mb"))
            action = A2P_ACTION_Y4MB;
        else if(!strcmp(argv[1], "packedraw"))
            action = A2P_ACTION_PACKEDRAW;
        else if(!strcmp(argv[1], "info"))
            action = A2P_ACTION_INFO;
        else if(!strcmp(argv[1], "x264bd"))
            action = A2P_ACTION_X264BD;
        else if(!strcmp(argv[1], "benchmark"))
            action = A2P_ACTION_BENCHMARK;
    }

    if(argc == 4) {
        if(action == A2P_ACTION_Y4MP ||
           action == A2P_ACTION_Y4MT ||
           action == A2P_ACTION_Y4MB) {
            sscanf(argv[2], "%d:%d", &sar_x, &sar_y);
            sar_x = abs(sar_x);
            sar_y = abs(sar_y);
                if(!(sar_x * sar_y) && (sar_x + sar_y))
                    action = A2P_ACTION_NOTHING;
        } else action = A2P_ACTION_NOTHING;
    }

    if(action == A2P_ACTION_NOTHING) {

        #ifdef A2P_AVS26
            fprintf(stderr, "avs2pipemod for AviSynth 2.6.0 Alpha 2");
        #else
            fprintf(stderr, "avs2pipemod for AviSynth 2.5.8");
        #endif

        fprintf(stderr,
                "  %s\n"
                "build on %s\n\n"
                "Usage: %s [option] input.avs\n"
                "   audio     - output wav extensible format audio to stdout.\n"
                "   aud16     - convert bit depth of audio to 16bit integer,\n"
                "               and output wav extensible format audio to stdout.\n"
                "   aud24     - convert bit depth of audio to 24bit integer,\n"
                "               and output wav extensible format audio to stdout.\n"
                "   y4mp [sar_x:sar_y  --  default 0:0]\n"
                "             - output yuv4mpeg2 format video to stdout as progressive.\n"
                "   y4mt [sar_x:sar_y  --  default 0:0]\n"
                "             - output yuv4mpeg2 format video to stdout as tff interlaced.\n"
                "   y4mb [sar_x:sar_y  --  default 0:0]\n"
                "             - output yuv4mpeg2 format video to stdout as bff interlaced.\n"
                "   packedraw - output rawvideo to stdout.\n"
                "               this mode accepts only packed format(RGB32|RGB24|YUY2).\n"
                "   info      - output information about aviscript clip.\n"
                "   x264bd    - suggest x264 arguments for bluray disc encoding.\n"
                "   benchmark - do benchmark and output results to stdout.\n"
                , A2PM_VERSION, A2PM_DATE_OF_BUILD, argv[0]);
        exit(2);
    }

    input = argv[3] ? argv[3] : argv[2];
    env = avs_create_script_environment(AVISYNTH_INTERFACE_VERSION);
    clip = avisynth_source(input, env);

    switch(action) {
        case A2P_ACTION_AUDIO:
            do_audio(clip, env, 0);
            break;
        case A2P_ACTION_AUD16:
            do_audio(clip, env, 16);
            break;
        case A2P_ACTION_AUD24:
            do_audio(clip, env, 24);
            break;
        case A2P_ACTION_Y4MP:
            do_y4m(clip, env, 'p', sar_x, sar_y);
            break;
        case A2P_ACTION_Y4MT:
            do_y4m(clip, env, 't', sar_x, sar_y);
            break;
        case A2P_ACTION_Y4MB:
            do_y4m(clip, env, 'b', sar_x, sar_y);
            break;
        case A2P_ACTION_PACKEDRAW:
            do_packedraw(clip, env);
            break;
        case A2P_ACTION_INFO:
            do_info(clip, env, input);
            break;
        case A2P_ACTION_X264BD:
            do_x264bd(clip, env);
            break;
        case A2P_ACTION_BENCHMARK:
            do_benchmark(clip, env, input);
            break;
        case A2P_ACTION_NOTHING: // Removing GCC warning, this action is handled above
            break;
    }

    avs_release_clip(clip);
    avs_delete_script_environment(env);

    exit(0);
}
