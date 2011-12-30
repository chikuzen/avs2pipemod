/*
 * avs2pipemod
 * Copyright (C) 2010-2011 Oka Motofumi <chikuzen.mo at gmail dot com>
 *                         Chris Beswick <chris.beswick@gmail.com>
 *
 * YUV4MPEG2 output inspired by Avs2YUV by Loren Merritt
 * Measurement of time and avs filter handling inspired by x264cli by x264 team
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

#include <fcntl.h>
#include <io.h>
#include <string.h>
#include <sys/timeb.h>
#include <time.h>
#include <getopt.h>

#include "common.h"
#include "wave.h"

#include "version.h"

#define BM_FRAMES_PAR_OUT       50
#define Y4M_FRAME_HEADER_STRING "FRAME\n"
#define Y4M_FRAME_HEADER_SIZE   6
#define BUFSIZE_OF_STDOUT       (128*1024) // 128KiB. The optimum value might be smaller than this.
#define A2PM_BT601              "Rec601"
#define A2PM_BT709              "Rec709"

enum a2p_action {
    A2P_ACTION_AUDIO,
    A2P_ACTION_Y4M,
    A2P_ACTION_RAWVIDEO,
    A2P_ACTION_INFO,
    A2P_ACTION_X264BD,
    A2P_ACTION_BENCHMARK,
    A2P_ACTION_NOTHING
};

typedef enum {
    A2PM_FALSE,
    A2PM_TRUE
} a2pm_bool;

typedef enum {
    A2PM_RAW,
    A2PM_NORMAL,
    A2PM_EXTRA,
    A2PM_EXTRA2
} fmt_type;

typedef struct {
    int sar_x;
    int sar_y;
    int start_frame;
    int end_frame;
    char ip;
    fmt_type fmt_type;
    char *bit;
    enum a2p_action action;
} params;

typedef struct {
    int num_planes; // Y8 -> 1, others -> 3
    int h_uv;       // horizontal subsampling of chroma
    int v_uv;       // vertial subsampling of chroma
    char *y4m_ctag_value;  // 
} yuvout;

static void parse_opts(int argc, char **argv, params *params);

static void usage();

#include "avisynth_filters.c"
#include "actions.c"

int __cdecl main(int argc, char **argv)
{
    params a2pm_params = {0, 0, 0, 0, 'p', A2PM_NORMAL, NULL, A2P_ACTION_NOTHING};
    parse_opts(argc, argv, &a2pm_params);

    if(a2pm_params.action == A2P_ACTION_NOTHING)
        usage();

    char *input = argv[argc - 1];
    AVS_ScriptEnvironment *env = avs_create_script_environment(AVISYNTH_INTERFACE_VERSION);
    AVS_Clip *clip = avisynth_source(input, env);

    switch(a2pm_params.action)
    {
        case A2P_ACTION_AUDIO:
            do_audio(clip, env, &a2pm_params);
            break;
        case A2P_ACTION_Y4M:
            do_y4m(clip, env, &a2pm_params);
            break;
        case A2P_ACTION_RAWVIDEO:
            do_rawvideo(clip, env, &a2pm_params);
            break;
        case A2P_ACTION_INFO:
            do_info(clip, env, input, A2PM_TRUE);
            break;
        case A2P_ACTION_X264BD:
            do_x264bd(clip, env, &a2pm_params);
            break;
        case A2P_ACTION_BENCHMARK:
            do_benchmark(clip, env, input, &a2pm_params);
            break;
        case A2P_ACTION_NOTHING: // Removing GCC warning, this action is handled above
            break;
    }

    avs_release_clip(clip);
    avs_delete_script_environment(env);

    exit(0);
}

static void parse_opts(int argc, char **argv, params *params)
{
    char short_opts[] = "a::Bb::e::ip::t::v::w::x::";
    struct option long_opts[] = {
        {"wav",      optional_argument, NULL, 'w'},
        {"extwav",   optional_argument, NULL, 'e'},
        {"rf64",     optional_argument, NULL, 'r'},
        {"audio",    optional_argument, NULL, 'e'},
        {"rawaudio", optional_argument, NULL, 'a'},
        {"y4mp",     optional_argument, NULL, 'p'},
        {"y4mt",     optional_argument, NULL, 't'},
        {"y4mb",     optional_argument, NULL, 'b'},
        {"rawvideo", optional_argument, NULL, 'v'},
        {"info",           no_argument, NULL, 'i'},
        {"x264bdp",  optional_argument, NULL, 'x'},
        {"x264bdt",  optional_argument, NULL, 'y'},
        {"benchmark",      no_argument, NULL, 'B'},
        {"trim",     required_argument, NULL, 'T'},
        {0,0,0,0}
    };
    int parse = 0;
    int index = 0;

    while((parse = getopt_long_only(argc, argv, short_opts, long_opts, &index)) != -1) {
        switch(parse) {
            case 'a':
            case 'e':
            case 'w':
            case 'r':
                params->action = A2P_ACTION_AUDIO;
                if(optarg)
                    params->bit = optarg;
                params->fmt_type = parse == 'e' ? A2PM_EXTRA :
                                   parse == 'r' ? A2PM_EXTRA2 :
                                   parse == 'a' ? A2PM_RAW :
                                                  A2PM_NORMAL;
                break;
            case 't':
            case 'b':
            case 'p':
                params->action = A2P_ACTION_Y4M;
                params->fmt_type = A2PM_NORMAL;
                if(optarg) {
                    sscanf(optarg, "%d:%d", &(params->sar_x), &(params->sar_y));
                    if(!(((params->sar_x > 0) && (params->sar_y >0)) ||
                          (!params->sar_x && !params->sar_y))) {
                        fprintf(stderr, "invalid argument \"%s\".\n\n", optarg);
                        params->action = A2P_ACTION_NOTHING;
                    }
                }
                params->ip = parse;
                break;
            case 'v':
                params->action = A2P_ACTION_RAWVIDEO;
                if(optarg && !strncmp(optarg, "vflip", 5))
                    params->fmt_type = A2PM_EXTRA;
                else
                    params->fmt_type = A2PM_RAW;
                break;
            case 'i':
                params->action = A2P_ACTION_INFO;
                break;
            case 'x':
            case 'y':
                params->action = A2P_ACTION_X264BD;
                params->ip = parse == 'x' ? 'p' : 't';
                params->sar_x = 1;
                params->sar_y = 1;
                if(optarg) {
                    if(!strncmp(optarg, "4:3", 3)) {
                        params->sar_x = 10;
                        params->sar_y = 11;
                    } else {
                        fprintf(stderr, "invalid argument \"%s\".\n\n", optarg);
                        params->action = A2P_ACTION_NOTHING;
                    }
                }
                break;
            case 'B':
                params->action = A2P_ACTION_BENCHMARK;
                break;
            case 'T':
                sscanf(optarg, "%d,%d", &params->start_frame, &params->end_frame);
                break;
            default:
                params->action = A2P_ACTION_NOTHING;
        }
    }
}

static void usage()
{
    #ifdef A2P_AVS26
        char *binary = "avs2pipe26mod";
        fprintf(stderr, "avs2pipemod for AviSynth 2.6.0 Alpha 3 or later");
    #else
        char *binary = "avs2pipemod";
        fprintf(stderr, "avs2pipemod for AviSynth 2.5x or later");
    #endif

    fprintf(stderr,
            "  %s\n"
            "build on %s\n"
            "\n"
            "Usage: %s [option] input.avs\n"
            "  e.g. avs2pipemod -wav=24bit input.avs > output.wav\n"
            "       avs2pipemod -y4mt=10:11 input.avs | x264 - --demuxer y4m -o tff.mkv\n"
            "       avs2pipemod -rawvideo -trim=1000,0 input.avs > output.yuv\n"
            "\n"
            "   -wav[=8bit|16bit|24bit|32bit|float  default unset]\n"
            "       output wav format audio to stdout.\n"
            "       if optional arg is set, bit depth of input will be converted\n"
            "      to specified value.\n"
            "\n"
            "   -extwav[=8bit|16bit|24bit|32bit|float  default unset]\n"
            "       output wav extensible format audio containing channel-mask to stdout.\n"
            "       if optional arg is set, bit depth of input will be converted\n"
            "      to specified value.\n"
            "\n"
            "   -rawaudio[=8bit|16bit|24bit|32bit|float  default unset]\n"
            "       output raw pcm audio(without any header) to stdout.\n"
            "       if optional arg is set, bit depth of input will be converted\n"
            "      to specified value.\n"
            "\n"
            "   -y4mp[=sar  default 0:0]\n"
            "       output yuv4mpeg2 format video to stdout as progressive.\n"
            "   -y4mt[=sar  default 0:0]\n"
            "       output yuv4mpeg2 format video to stdout as tff interlaced.\n"
            "   -y4mb[=sar  default 0:0]\n"
            "       output yuv4mpeg2 format video to stdout as bff interlaced.\n"
            "\n"
            "   -rawvideo[=vflip default unset]\n"
            "       output rawvideo(without any header) to stdout.\n"
            "\n"
            "   -x264bdp[=4:3  default unset(16:9)]\n"
            "       suggest x264(r1939 or later) arguments for bluray disc encoding\n"
            "      in case of progressive source.\n"
            "       set optarg if DAR4:3 is required(ntsc/pal sd source).\n"
            "   -x264bdt[=4:3  default unset(16:9)]\n"
            "       suggest x264(r1939 or later) arguments for bluray disc encoding\n"
            "      in case of tff source.\n"
            "       set optarg if DAR4:3 is required(ntsc/pal sd source).\n"
            "\n"
            "   -info  - output information about aviscript clip.\n"
            "\n"
            "   -benchmark - do benchmark aviscript, and output results to stdout.\n"
            "\n"
            "   -trim[=first_frame,last_frame  default 0,0]\n"
            "       add Trim(first_frame,last_frame) to input script.\n"
            "       in info/x264bd, this option is ignored.\n"
            "\n"
            "note  : in yuv4mpeg2 output mode, RGB input that has 720pix height or more\n"
            "       will be converted to YUV with Rec.709 coefficients instead of Rec.601.\n"
            "\n"
            "note2 : '-x264bdp/t' supports only for primary stream encoding.\n"
            "\n"
            "note3 : '-extwav' supports only general speaker positions.\n"
            " Chan. MS channels                Description\n"
            " ----- -------------------------  ----------------\n"
            "  1    FC                         Mono\n"
            "  2    FL FR                      Stereo\n"
            "  3    FL FR BC                   First Surround\n"
            "  4    FL FR BL BR                Quadro\n"
            "  5    FL FR FC BL BR             like Dpl II (without LFE)\n"
            "  6    FL FR FC LF BL BR          Standard Surround\n"
            "  7    FL FR FC LF BL BR BC       With back center\n"
            "  8    FL FR FC LF BL BR FLC FRC  With front center left/right\n"
            , A2PM_VERSION, A2PM_DATE_OF_BUILD, binary);
    exit(2);
}