/*
 * Copyright (C) 2012 Oka Motofumi <chikuzen.mo at gmail dot com>
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

#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>

#include "avs2pipemod.h"
#include "avs_func.h"

extern int act_do_video(params_t *pr, avs_hnd_t *ah, AVS_Value res);
extern int act_do_audio(params_t *pr, avs_hnd_t *ah, AVS_Value res);
extern int act_do_info(params_t *pr, avs_hnd_t *ah);
extern int act_do_benchmark(params_t *pr, avs_hnd_t *ah, AVS_Value res);
extern int act_do_x264bd(params_t *pr, avs_hnd_t *ah, AVS_Value res);
extern int act_do_x264raw(params_t *pr, avs_hnd_t *ah, AVS_Value res);
extern int act_dump_pix_values_as_txt(params_t *pr, avs_hnd_t *ah, AVS_Value res);

static float get_avisynth_version(avs_hnd_t *ah)
{
    RETURN_IF_ERROR(!ah->func.avs_function_exists(ah->env, "VersionNumber" ), -1, "VersionNumber does not exist\n");
    AVS_Value ver = ah->func.avs_invoke(ah->env, "VersionNumber", avs_new_value_array(NULL, 0), NULL);
    RETURN_IF_ERROR(avs_is_error(ver), -1, "unable to determine avisynth version: %s\n", avs_as_error(ver));
    RETURN_IF_ERROR(!avs_is_float(ver), -1, "VersionNumber did not return a float value\n");
    float ret = avs_as_float(ver);
    ah->func.avs_release_value(ver);
    return ret;
}

#define LOAD_AVS_FUNC(name, continue_on_fail)\
{\
    ah->func.name = (void*)GetProcAddress(ah->library, #name);\
    if( !continue_on_fail && !ah->func.name )\
        goto fail;\
}

static int load_avisynth_dll(avs_hnd_t *ah)
{
    ah->library = LoadLibrary("avisynth");
    if(!ah->library)
        return -1;
    LOAD_AVS_FUNC(avs_clip_get_error, 0 );
    LOAD_AVS_FUNC(avs_create_script_environment, 0);
    LOAD_AVS_FUNC(avs_delete_script_environment, 1);
    LOAD_AVS_FUNC(avs_get_error, 1);
    LOAD_AVS_FUNC(avs_get_frame, 0);
    LOAD_AVS_FUNC(avs_get_audio, 0);
    LOAD_AVS_FUNC(avs_get_version, 0);
    LOAD_AVS_FUNC(avs_get_video_info, 0);
    LOAD_AVS_FUNC(avs_function_exists, 0);
    LOAD_AVS_FUNC(avs_invoke, 0);
    LOAD_AVS_FUNC(avs_bit_blt, 0);
    LOAD_AVS_FUNC(avs_release_clip, 0);
    LOAD_AVS_FUNC(avs_release_value, 0);
    LOAD_AVS_FUNC(avs_release_video_frame, 0);
    LOAD_AVS_FUNC(avs_take_clip, 0);
    return 0;
fail:
    FreeLibrary(ah->library);
    return -1;
}

static AVS_Value initialize_avisynth(params_t *pr, avs_hnd_t *ah)
{
    RETURN_IF_ERROR(load_avisynth_dll(ah), avs_void, "failed to load avisynth.dll\n");

    ah->env = ah->func.avs_create_script_environment(AVS_INTERFACE_25);
    if (ah->func.avs_get_error) {
        const char *error = ah->func.avs_get_error(ah->env);
        RETURN_IF_ERROR(error, avs_void, "%s\n", error);
    }

    ah->version = get_avisynth_version(ah);
    RETURN_IF_ERROR(ah->version < 2.5, avs_void, "couldn't find valid version of avisynth.dll\n");

    AVS_Value arg = avs_new_value_string(pr->input);
    AVS_Value res = ah->func.avs_invoke(ah->env, "Import", arg, NULL);
    RETURN_IF_ERROR(avs_is_error(res), res, "%s\n", avs_as_string(res));
#ifdef BLAME_THE_FLUFF /* see http://mod16.org/hurfdurf/?p=234 */
    AVS_Value mt_test = ah->func.avs_invoke(ah->env, "GetMTMode", avs_new_value_bool(0), NULL);
    int mt_mode = avs_is_int(mt_test) ? avs_as_int(mt_test) : 0;
    ah->func.avs_release_value(mt_test);
    if (mt_mode > 0 && mt_mode < 5) {
        AVS_Value temp = ah->func.avs_invoke(ah->env, "Distributor", res, NULL);
        ah->func.avs_release_value(res);
        res = temp;
    }
#endif
    RETURN_IF_ERROR(!avs_is_clip(res), res, "'%s' didn't return a varid clip\n", pr->input);
    ah->clip = ah->func.avs_take_clip(res, ah->env);
    ah->vi = ah->func.avs_get_video_info(ah->clip);

    return res;
}

static int close_avisynth_dll(avs_hnd_t *ah)
{
    ah->func.avs_release_clip(ah->clip);
    if(ah->func.avs_delete_script_environment) {
        ah->func.avs_delete_script_environment(ah->env);
    }
    FreeLibrary(ah->library);
    return 0;
}

static void parse_opts(int argc, char **argv, params_t *p)
{
    char short_opts[] = "a::Bb::c::C::de::ip::t::T:v::w::x::y::";
    struct option long_opts[] = {
        {"rawaudio" , optional_argument, NULL, 'a'},
        {"extwav"   , optional_argument, NULL, 'e'},
        {"audio"    , optional_argument, NULL, 'e'}, /* for backward compatibility */
        {"wav"      , optional_argument, NULL, 'w'},
        //{"rf64",     optional_argument, NULL, 'r'},
        {"y4mp"     , optional_argument, NULL, 'p'},
        {"video"    , optional_argument, NULL, 'p'}, /* for backward compatibility */
        {"y4mt"     , optional_argument, NULL, 't'},
        {"y4mb"     , optional_argument, NULL, 'b'},
        {"rawvideo" , optional_argument, NULL, 'v'},
        {"info"     ,       no_argument, NULL, 'i'},
        {"x264bdp"  , optional_argument, NULL, 'x'},
        {"x264bdt"  , optional_argument, NULL, 'y'},
        {"benchmark",       no_argument, NULL, 'B'},
        {"x264raw"  , optional_argument, NULL, 'c'},
        {"x264rawtc", optional_argument, NULL, 'C'},
        {"dumpyuv"  ,       no_argument, NULL, 'd'}, /* for backward compatibility */
        {"dumptxt"  ,       no_argument, NULL, 'd'},
        {"trim"     , required_argument, NULL, 'T'},
        {0, 0, 0, 0}
    };

    int index = 0;
    int parse = 0;
    p->action = A2PM_ACT_NOTHING;
    while ((parse = getopt_long_only(argc, argv, short_opts, long_opts, &index)) != -1) {
        switch(parse) {
        case 'a':
        case 'e':
        case 'w':
        //case 'r':
            p->action = A2PM_ACT_AUDIO;
            if(optarg)
                p->bit = optarg;
            p->format_type = parse == 'e' ? A2PM_FMT_WAVEFORMATEXTENSIBLE :
                             //parse == 'r' ? A2PM_FMT_RF64 :
                             parse == 'a' ? A2PM_FMT_RAWAUDIO :
                                            A2PM_FMT_WAVEFORMATEX;
            break;
        case 't':
        case 'b':
        case 'p':
            p->action = A2PM_ACT_VIDEO;
            p->format_type = A2PM_FMT_YUV4MPEG2;
            p->frame_type = parse;
            if (!optarg)
                break;
            sscanf(optarg, "%d:%d", &p->sar[0], &p->sar[1]);
            if (p->sar[0] < 0 || p->sar[1] < 0) {
                a2pm_log(A2PM_LOG_ERROR, "invalid argument \"%s\".\n\n", optarg);
                p->action = A2PM_ACT_NOTHING;
            }
            break;
        case 'v':
            p->action = A2PM_ACT_VIDEO;
            p->format_type = A2PM_FMT_RAWVIDEO;
            if (!optarg)
                break;
            if (!strncmp(optarg, "vflip", 5))
                p->format_type = A2PM_FMT_RAWVIDEO_VFLIP;
            break;
        case 'i':
            p->action = A2PM_ACT_INFO;
            break;
        case 'x':
        case 'y':
            p->action = A2PM_ACT_X264BD;
            p->frame_type = parse == 'x' ? 'p' : 't';
            p->format_type = A2PM_FMT_HDBD;
            if (!optarg)
                break;
            if(strncmp(optarg, "4:3", 3)) {
                a2pm_log(A2PM_LOG_ERROR, "invalid argument \"%s\".\n\n", optarg);
                p->action = A2PM_ACT_NOTHING;
            }
            p->format_type = A2PM_FMT_SDBD;
            break;
        case 'c':
        case 'C':
            p->action = A2PM_ACT_X264RAW;
            p->yuv_depth = 8;
            p->format_type = parse == 'c' ? A2PM_FMT_WITHOUT_TCFILE : A2PM_FMT_WITH_TCFILE;
            if (!optarg)
                break;
            sscanf(optarg, "%d", &p->yuv_depth);
            if (p->yuv_depth < 8 || p->yuv_depth > 16) {
                a2pm_log(A2PM_LOG_ERROR, "invalid argument \"%s\".\n", optarg);
                a2pm_log(A2PM_LOG_ERROR, "the argument needs to be an integer of 8 to 16.\n\n");
                p->action = A2PM_ACT_NOTHING;
            }
            break;
        case 'B':
            p->action = A2PM_ACT_BENCHMARK;
            break;
        case 'd':
            p->action = A2PM_ACT_DUMP_PIXEL_VALUES_AS_TXT;
            break;
        case 'T':
            sscanf(optarg, "%d,%d", &p->trim[0], &p->trim[1]);
            break;
        default:
            break;
        }
    }
}

static void usage()
{
    fprintf(stderr,
#ifdef BLAME_THE_FLUFF
            "avs2pipemod  ver %s     with MT support\n"
#else
            "avs2pipemod  ver %s\n"
#endif
            "built on %s %s\n"
            "\n"
            "Usage: avs2pipemod [option] input.avs\n"
            "  e.g. avs2pipemod -wav=24bit input.avs > output.wav\n"
            "       avs2pipemod -y4mt=10:11 input.avs | x264 - --demuxer y4m -o tff.mkv\n"
            "       avs2pipemod -rawvideo -trim=1000,0 input.avs > output.yuv\n"
            "\n"
            "   -wav[=8bit|16bit|24bit|32bit|float  default unset]\n"
            "        output wav format audio(WAVEFORMATEX) to stdout.\n"
            "        if optional arg is set, audio sample type of input will be converted\n"
            "        to specified value.\n"
            "\n"
            "   -extwav[=8bit|16bit|24bit|32bit|float  default unset]\n"
            "        output wav extensible format audio(WAVEFORMATEXTENSIBLE) containing\n"
            "        channel-mask to stdout.\n"
            "        if optional arg is set, audio sample type of input will be converted\n"
            "        to specified value.\n"
            "\n"
            "   -rawaudio[=8bit|16bit|24bit|32bit|float  default unset]\n"
            "        output raw pcm audio(without any header) to stdout.\n"
            "        if optional arg is set, audio sample type of input will be converted\n"
            "        to specified value.\n"
            "\n"
            "   -y4mp[=sar  default 0:0]\n"
            "        output yuv4mpeg2 format video to stdout as progressive.\n"
            "   -y4mt[=sar  default 0:0]\n"
            "        output yuv4mpeg2 format video to stdout as tff interlaced.\n"
            "   -y4mb[=sar  default 0:0]\n"
            "        output yuv4mpeg2 format video to stdout as bff interlaced.\n"
            "\n"
            "   -rawvideo[=vflip default unset]\n"
            "        output rawvideo(without any header) to stdout.\n"
            "\n"
            "   -x264bdp[=4:3  default unset(16:9)]\n"
            "        suggest x264(r1939 or later) arguments for bluray disc encoding\n"
            "        in case of progressive source.\n"
            "        set optarg if DAR4:3 is required(ntsc/pal sd source).\n"
            "   -x264bdt[=4:3  default unset(16:9)]\n"
            "        suggest x264(r1939 or later) arguments for bluray disc encoding\n"
            "        in case of tff interlaced source.\n"
            "        set optional arg if DAR4:3 is required(ntsc/pal sd source).\n"
            "\n"
            "   -x264raw[=input-depth(8 to 16) default 8]\n"
            "        suggest x264 arguments in case of -rawvideo output.\n"
            "        set optional arg when using interleaved output of dither hack.\n"
            "   -x264rawtc[=input-depth(8 to 16) default 8]\n"
            "        suggest x264 arguments in case of -rawvideo output with --tcfile-in.\n"
            "        set optional arg when using interleaved output of dither hack.\n"
            "\n"
            "   -info  - output information about aviscript clip.\n"
            "\n"
            "   -benchmark - do benchmark aviscript, and output results to stdout.\n"
            "\n"
            "   -dumptxt - dump pixel values as tab separated text to stdout.\n"
            "\n"
            "   -trim[=first_frame,last_frame  default 0,0]\n"
            "        add Trim(first_frame,last_frame) to input script.\n"
            "        in info, this option is ignored.\n"
            "\n"
            "note1 : in yuv4mpeg2 output mode, RGB input that has 720pix height or more\n"
            "        will be converted to YUV with Rec.709 coefficients instead of Rec.601.\n"
            "        and, if your avisynth version is 2.5.x, YUY2 input will be converted\n"
            "        to YV12.\n"
            "\n"
            "note2 : '-x264bd(p/t)' supports only for primary stream encoding.\n"
            "\n"
            "note3 : in fact, it is a spec violation to use WAVEFORMATEX(-wav option)\n"
            "        except 8bit/16bit PCM.\n"
            "        however, there are some applications that accept such invalid files\n"
            "        instead of supporting WAVEFORMATEXTENSIBLE.\n"
            "\n"
            "note4 : '-extwav' supports only general speaker positions.\n"
            "\n"
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
            "\n"
            "note5 : in '-x264raw(tc)' with dither hack, output format needs to be\n"
            "        interleaved(not stacked).\n",
            A2PM_VERSION, __DATE__, __TIME__);
}

int main(int argc, char **argv)
{
    int retcode = -1;

    if (argc < 3) {
        usage();
        goto ret;
    }

    params_t params = {0};
    parse_opts(argc, argv, &params);

    if (params.action == A2PM_ACT_NOTHING) {
        usage();
        goto ret;
    }
    params.input = argv[argc - 1];

    avs_hnd_t avs_h = {0};
    AVS_Value res = initialize_avisynth(&params, &avs_h);
    if (!avs_is_clip(res))
        goto close;

    switch(params.action) {
    case A2PM_ACT_AUDIO:
        retcode = act_do_audio(&params, &avs_h, res);
        break;
    case A2PM_ACT_VIDEO:
        retcode = act_do_video(&params, &avs_h, res);
        break;
    case A2PM_ACT_INFO:
        retcode = act_do_info(&params, &avs_h);
        break;
    case A2PM_ACT_X264BD:
        retcode = act_do_x264bd(&params, &avs_h, res);
        break;
    case A2PM_ACT_X264RAW:
        retcode = act_do_x264raw(&params, &avs_h, res);
        break;
    case A2PM_ACT_BENCHMARK:
        retcode = act_do_benchmark(&params, &avs_h, res);
        break;
    case A2PM_ACT_DUMP_PIXEL_VALUES_AS_TXT:
        retcode = act_dump_pix_values_as_txt(&params, &avs_h, res);
        break;
    default:
        break;
    }
    avs_h.func.avs_release_value(res);

close:
    if (avs_h.library)
        close_avisynth_dll(&avs_h);
ret:
    exit(retcode);
}
