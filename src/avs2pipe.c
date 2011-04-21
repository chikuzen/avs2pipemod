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

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <fcntl.h>
#include <io.h>
#include <string.h>
#ifdef A2P_AVS26
    #include "avisynth26/avisynth_c.h"
#else
    #include "avisynth25/avisynth_c.h"
#endif
#include "common.h"
#include "wave.h"

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
do_audio(AVS_Clip *clip, AVS_ScriptEnvironment *env)
{
    const AVS_VideoInfo *info;
    WaveRiffHeader *header;
    WaveFormatType format;
    void *buff;
    size_t size, count, step;
    uint64_t i, wrote, target;

    info = avs_get_video_info(clip);

    if(!avs_has_audio(info)) {
        a2p_log(A2P_LOG_ERROR, "clip has no audio.\n", 0);
    }

    if(info->sample_type == AVS_SAMPLE_FLOAT) {
        format = WAVE_FORMAT_IEEE_FLOAT;
    } else {
        format = WAVE_FORMAT_PCM;
    }

    if(_setmode(_fileno(stdout), _O_BINARY) == -1) {
        a2p_log(A2P_LOG_ERROR, "cannot switch stdout to binary mode.\n", 0);
    }

    a2p_log(A2P_LOG_INFO, "writing %I64d seconds of %d Hz, %d channel audio.\n",
            (info->num_audio_samples / info->audio_samples_per_second),
            info->audio_samples_per_second, info->nchannels);

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
        if(target - i < count) count = (size_t) (target - i);
        avs_get_audio(clip, buff, i, count);
        step = fwrite(buff, size, count, stdout);
        // fail early if there is a problem instead of end of input
        if(step != count) break;
        wrote += count;
        //a2p_log(A2P_LOG_REPEAT, "written %lld seconds [%lld%%]... ", 
        //    wrote / info->audio_samples_per_second,
        //    (100 * wrote) / target);
    }
    fflush(stdout); // clear buffers before we exit
    a2p_log(A2P_LOG_REPEAT, "finished, wrote %I64u seconds [%I64u%%].\n", 
        wrote / info->audio_samples_per_second,
        (100 * wrote) / target);
    free(buff);
    free(header);
    if(wrote != target) {
        a2p_log(A2P_LOG_ERROR, "only wrote %I64u of %I64u samples.\n",
                wrote, target);
    }
}

void
do_video(AVS_Clip *clip, AVS_ScriptEnvironment *env)
{
    const AVS_VideoInfo *info;
    AVS_VideoFrame *frame;
    static const int planes[] = {AVS_PLANAR_Y, AVS_PLANAR_U, AVS_PLANAR_V};
    const BYTE *buff; // BYTE from avisynth_c.h not windows headers
    char * yuv_csp;
    size_t count, step;
    int32_t w, h, f, p, i, pitch, h_uv, v_uv, np;
    int32_t wrote, target;

    info = avs_get_video_info(clip);

    if(!avs_has_video(info)) {
        a2p_log(A2P_LOG_ERROR, "clip has no video.\n");
    }

    // Number of planes normally 3;
    np = 3;

	// Setup Correct Colorspace Handling
	// Thanks to Chikuzen @ Doom9 Forums
    switch(info->pixel_type) {
        #ifdef A2P_AVS26
        case AVS_CS_BGR32:
        case AVS_CS_BGR24:
            a2p_log(A2P_LOG_INFO, "converting video to yv24.\n", 0);
            clip = avisynth_filter(clip, env, "ConvertToYV24");
            info = avs_get_video_info(clip);
        case AVS_CS_YV24:
            yuv_csp = "444";
            count = info->width * info->height * 3;
            h_uv = 0;
            v_uv = 0;
            break;
        case AVS_CS_YUY2:
            a2p_log(A2P_LOG_INFO, "converting video to yv16.\n", 0);
            clip = avisynth_filter(clip, env, "ConvertToYV16");
            info = avs_get_video_info(clip);
        case AVS_CS_YV16:
            yuv_csp = "422";
            count = info->width * info->height * 2;
            h_uv = 1;
            v_uv = 0;
            break;
        case AVS_CS_YV411:
            yuv_csp = "411";
            count = info->width * info->height * 3 / 2;
            h_uv = 2;
            v_uv = 0;
            break;
        case AVS_CS_Y8:
            yuv_csp = "mono";
            count = info->width * info->height;
            h_uv = 0;
            v_uv = 0;
            np = 1; // special case only one plane for mono
            break;
        #endif        
        default:
            a2p_log(A2P_LOG_INFO, "converting video to yv12.\n", 0);
            clip = avisynth_filter(clip, env, "ConvertToYV12");
            info = avs_get_video_info(clip);
        case AVS_CS_YV12:
            yuv_csp = "420";
            count = info->width * info->height * 3 / 2;
            h_uv = 1;
            v_uv = 1;
    }

    if(_setmode(_fileno(stdout), _O_BINARY) == -1) {
        a2p_log(A2P_LOG_ERROR, "cannot switch stdout to binary mode.\n");
    }

    a2p_log(A2P_LOG_INFO, "writing %d frames of %d/%d fps, %dx%d YUV%s %s video.\n",
            info->num_frames, info->fps_numerator, info->fps_denominator,
            info->width, info->height, yuv_csp, !avs_is_field_based(info) ?
             "progressive" : !avs_is_bff(info) ? "tff" : "bff"); // default tff

    // YUV4MPEG2 header http://wiki.multimedia.cx/index.php?title=YUV4MPEG2
    fprintf(stdout, "YUV4MPEG2 W%d H%d F%u:%u I%s A0:0 C%s\n", info->width,
            info->height, info->fps_numerator, info->fps_denominator,
            !avs_is_field_based(info) ? "p" : !avs_is_bff(info) ? "t" : "b",
            yuv_csp);
    fflush(stdout);
    
    // method from avs2yuv converted to c
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
        // not sure release is needed, but it doesn't cause an error
        avs_release_frame(frame);
        // fail early if there is a problem instead of end of input
        if(step != count) break;
        wrote++;
        //a2p_log(A2P_LOG_REPEAT, "written %d frames [%d%%]... ", 
        //    wrote, (100 * wrote) / target);
    }
    fflush(stdout); // clear buffers before we exit
    a2p_log(A2P_LOG_REPEAT, "finished, wrote %d frames [%d%%].\n", 
        wrote, (100 * wrote) / target);
    if(wrote != target) {
        a2p_log(A2P_LOG_ERROR, "only wrote %d of %d frames.\n", wrote, target);
    }
}

void
do_info(AVS_Clip *clip, AVS_ScriptEnvironment *env)
{
    const AVS_VideoInfo *info;

    info = avs_get_video_info(clip);

    if(avs_has_video(info)) {
        fprintf(stdout, "v:width       %d\n", info->width);
        fprintf(stdout, "v:height      %d\n", info->height);
        fprintf(stdout, "v:fps         %d/%d\n",
                info->fps_numerator, info->fps_denominator);
        fprintf(stdout, "v:frames      %d\n", info->num_frames);
        fprintf(stdout, "v:duration    %d\n",
                info->num_frames * info->fps_denominator / info->fps_numerator);
        fprintf(stdout, "v:interlaced  %s\n", !avs_is_field_based(info) ?
                "no" : !avs_is_bff(info) ? "tff" : "bff");
        fprintf(stdout, "v:pixel_type  %x\n", info->pixel_type);
    }
    if(avs_has_audio(info)) {
        fprintf(stdout, "a:sample_rate %d\n", info->audio_samples_per_second);
        fprintf(stdout, "a:format      %s\n",
                info->sample_type == AVS_SAMPLE_FLOAT ? "float" : "pcm");
        fprintf(stdout, "a:bit_depth   %d\n",
                avs_bytes_per_channel_sample(info) * 8);
        fprintf(stdout, "a:channels    %d\n", info->nchannels);
        fprintf(stdout, "a:samples     %I64d\n", info->num_audio_samples);
        fprintf(stdout, "a:duration    %I64d\n",
                info->num_audio_samples / info->audio_samples_per_second);
    }
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

    fprintf(stdout, "--weightp 1 --bframes 3 --nal-hrd vbr --vbv-maxrate 15000"
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
    char * input;
    enum {
        A2P_ACTION_AUDIO,
        A2P_ACTION_VIDEO,
        A2P_ACTION_INFO,
        A2P_ACTION_X264BD,
        A2P_ACTION_NOTHING    
    } action;

    action = A2P_ACTION_NOTHING;

    if(argc == 3) {
        if(strcmp(argv[1], "audio") == 0) {
            action = A2P_ACTION_AUDIO;
        } else if(strcmp(argv[1], "video") == 0) {
            action = A2P_ACTION_VIDEO;
        } else if(strcmp(argv[1], "info") == 0) {
            action = A2P_ACTION_INFO;
        } else if(strcmp(argv[1], "x264bd") == 0) {
            action = A2P_ACTION_X264BD;
        }
        input = argv[2];
    }

    if(action == A2P_ACTION_NOTHING) {
       
        #ifdef A2P_AVS26
            fprintf(stderr, "avs2pipe for AviSynth 2.6.0 Alpha 2\n");
        #else
            fprintf(stderr, "avs2pipe for AviSynth 2.5.8\n");
        #endif
        fprintf(stderr, "Usage: avs2pipe [audio|video|info|x264] input.avs\n");
        fprintf(stderr, "   audio  - output wav extensible format audio to stdout.\n");
        fprintf(stderr, "   video  - output yuv4mpeg2 format video to stdout.\n");
        fprintf(stderr, "   info   - output information about aviscript clip.\n");
        fprintf(stderr, "   x264bd - suggest x264 arguments for bluray disc encoding.\n");
        exit(2);
    }

    env = avs_create_script_environment(AVISYNTH_INTERFACE_VERSION);
    clip = avisynth_source(input, env);

    switch(action) {
        case A2P_ACTION_AUDIO:
            do_audio(clip, env);
            break;
        case A2P_ACTION_VIDEO:
            do_video(clip, env);
            break;
        case A2P_ACTION_INFO:
            do_info(clip, env);
            break;
        case A2P_ACTION_X264BD:
            do_x264bd(clip, env);
            break;
        case A2P_ACTION_NOTHING: // Removing GCC warning, this action is handled above
            break;
    }

    avs_release_clip(clip);
    avs_delete_script_environment(env);

    exit(0);
}
