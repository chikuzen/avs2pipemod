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

#include <memory>
#include "avs2pipemod.h"
#include "utils.h"
#include "getopt.h"
#include <format>



static void usage()
{
    fprintf(stderr, "avs2pipemod  ver %s\nbuilt on %s %s\n\n",
            A2PM_VERSION, __DATE__, __TIME__);
    fprintf(stderr,
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
#if 0
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
#endif
"   -info  - output information about aviscript clip.\n"
"\n"
"   -filters - output external plugin filters/functions list to stdout.\n"
"\n"
"   -benchmark - do benchmark avs script, and output results to stdout.\n"
"\n"
"   -dumptxt - dump pixel values as tab separated text to stdout.\n"
"\n"
"   -trim[=first_frame,last_frame  default 0,0]\n"
"        add Trim(first_frame,last_frame) to input script.\n"
"        in info, this option is ignored.\n"
"\n"
"   -dll[=path to avisynth.dll  default \"avisynth\"]"
"        specify which avisynth.dll is used.\n"
"note1 : in yuv4mpeg2 output mode, RGB input that has 720pix height or more\n"
"        will be converted to YUV with Rec.709 coefficients instead of Rec.601.\n"
"\n"
"note2 : in fact, it is a spec violation to use WAVEFORMATEX(-wav option)\n"
"        except 8bit/16bit PCM.\n"
"        however, there are some applications that accept such invalid files\n"
"        instead of supporting WAVEFORMATEXTENSIBLE.\n"
"\n"
"note3 : speaker positions in files output by '-extwav' are set using\n"
"        the clip property from avs+.\n"
"        if it is not set, general speaker positions are used instead.\n"
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
#if 0
"note4 : '-x264bd(p/t)' supports only for primary stream encoding.\n"
"\n"
"note5 : in '-x264raw(tc)' with dither hack, output format needs to be\n"
"        interleaved(not stacked).\n"
#endif
);

}


static void parse_opts(int argc, char **argv, Params& p)
{
    char short_opts[] = "a::Bb::c::C::de::ip::t::T:v::w::x::y::";
    struct option long_opts[] = {
        { "rawaudio", optional_argument, nullptr, 'a' },
        { "extwav", optional_argument, nullptr, 'e' },
        { "audio", optional_argument, nullptr, 'e' }, /* for backward compatibility */
        { "wav", optional_argument, nullptr, 'w' },
        { "y4mp", optional_argument, nullptr, 'p' },
        { "video", optional_argument, nullptr, 'p' }, /* for backward compatibility */
        { "y4mt", optional_argument, nullptr, 't' },
        { "y4mb", optional_argument, nullptr, 'b' },
        { "rawvideo", optional_argument, nullptr, 'v' },
        { "info", no_argument, nullptr, 'i' },
        { "benchmark", no_argument, nullptr, 'B' },
#if 0
        { "x264bdp", optional_argument, nullptr, 'x' },
        { "x264bdt", optional_argument, nullptr, 'y' },
        { "x264raw", optional_argument, nullptr, 'c' },
        { "x264rawtc", optional_argument, nullptr, 'C' },
#endif
        { "dumpyuv", no_argument, nullptr, 'd' }, /* for backward compatibility */
        { "dumptxt", no_argument, nullptr, 'd' },
        { "filters", no_argument, nullptr, 'f' },
        { "trim", required_argument, nullptr, 'T' },
        { "dll", required_argument, nullptr, 'D' },
        { "y4mbits", required_argument, nullptr, 'Y'},
        {nullptr, 0, nullptr, 0}
    };

    int index = 0;
    int parse = 0;
    int ret;

    while ((parse = getopt_long_only(argc, argv, short_opts, long_opts, &index)) != -1) {
        switch(parse) {
        case 'a':
        case 'e':
        case 'w':
            p.action = A2PM_ACT_AUDIO;
            if(optarg)
                p.bit = optarg;
            p.format_type = parse == 'e' ? FMT_WAVEFORMATEXTENSIBLE :
                            parse == 'a' ? FMT_RAWAUDIO : FMT_WAVEFORMATEX;
            break;
        case 't':
        case 'b':
        case 'p':
            p.action = A2PM_ACT_VIDEO;
            p.format_type = FMT_YUV4MPEG2;
            p.frame_type = parse;
            if (optarg) {
                ret = sscanf(optarg, "%d:%d", &p.sarnum, &p.sarden);
                validate(p.sarnum < 0 || p.sarden < 0,
                         std::format("invalid argument \"{}\".\n\n", optarg));
            }
            break;
        case 'v':
            p.action = A2PM_ACT_VIDEO;
            p.format_type = FMT_RAWVIDEO;
            if (optarg && !strncmp(optarg, "vflip", 5))
                p.format_type = FMT_RAWVIDEO_VFLIP;
            break;
        case 'i':
            p.action = A2PM_ACT_INFO;
            break;
        case 'f':
            p.action = A2PM_ACT_FILTERS;
            break;
        case 'B':
            p.action = A2PM_ACT_BENCHMARK;
            break;
#if 0
        case 'x':
        case 'y':
            p.action = A2PM_ACT_X264BD;
            p.frame_type = parse == 'x' ? 'p' : 't';
            p.format_type = FMT_HDBD;
            if (optarg) {
                validate(strncmp(optarg, "4:3", 3) != 0,
                         "invalid argument \"%s\".\n\n", optarg);
                p.format_type = FMT_SDBD;
            }
            break;
        case 'c':
        case 'C':
            p.action = A2PM_ACT_X264RAW;
            p.yuv_depth = 8;
            p.format_type = parse == 'c' ? FMT_WITHOUT_TCFILE : FMT_WITH_TCFILE;
            if (!optarg)
                break;
            sscanf(optarg, "%d", &p.yuv_depth);
            if (p.yuv_depth < 8 || p.yuv_depth > 16) {
                fprintf(stderr, "avs2pipemod[error]:invalid argument \"%s\".\n\n", optarg);
                fprintf(stderr, "The argument needs to be an integer of 8 to 16.\n\n");
                throw std::runtime_error(nullptr);
            }
            break;
#endif
        case 'd':
            p.action = A2PM_ACT_DUMP_PIXEL_VALUES_AS_TXT;
            break;
        case 'T':
            ret = sscanf(optarg, "%d,%d", &p.trimstart, &p.trimend);
            break;
        case 'D':
            p.dll_path = optarg;
            break;
        case 'Y':
            ret = sscanf(optarg, "%d", &p.yuv_depth);
            validate(p.yuv_depth != 9 && p.yuv_depth != 10 && p.yuv_depth != 12
                        && p.yuv_depth != 14 && p.yuv_depth != 16,
                     "invalid bits specified.");
            break;
        default:
            break;
        }
    }
}



int main(int argc, char** argv)
{
    if (argc < 3) {
        usage();
        return -1;
    }

    try {
        auto params = Params();
        parse_opts(argc, argv, params);

        std::unique_ptr<Avs2PipeMod> a2pm(
            Avs2PipeMod::create(argv[argc - 1], params));

        switch(params.action) {
        case A2PM_ACT_INFO:
            a2pm->info(true);
            break;
        case A2PM_ACT_BENCHMARK:
            a2pm->benchmark(params);
            break;
        case A2PM_ACT_AUDIO:
            a2pm->outAudio(params);
            break;
        case A2PM_ACT_VIDEO:
            a2pm->outVideo(params);
            break;
#if 0
        case A2PM_ACT_X264BD:
            a2pm->x264bd(params);
            break;
        case A2PM_ACT_X264RAW:
            a2pm->x264raw(params);
            break;
#endif
        case A2PM_ACT_DUMP_PIXEL_VALUES_AS_TXT:
            a2pm->dumpPixValues(params);
            break;
        case A2PM_ACT_FILTERS:
            a2pm->dumpPluginFiltersList(params);
            break;
        default:
            break;
        }
    } catch (std::runtime_error& e) {
        if (e.what()) {
            fprintf(stderr, "avs2pipemod[error]: %s", e.what());
        }
        exit(-1);
    } catch (AvisynthError& e) {
        fprintf(stderr, "avs2pipemod[error]: %s", e.msg);
        exit(-1);
    }

    return 0;
}
