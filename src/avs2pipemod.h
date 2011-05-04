#ifndef AVS2PIPEMOD_H
#define AVS2PIPEMOD_H

#include <fcntl.h>
#include <io.h>
#include <string.h>
#include <sys/timeb.h>
#include <time.h>
#include <getopt.h>

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
#define BUFSIZE_OF_STDOUT 262144 // 256kB. The optimum value might be smaller than this.
#define A2PM_BT601 "Rec601"
#define A2PM_BT709 "Rec709"
#define NEED_AUDIO_INFO 1
#define NOT_NEED_AUDIO_INFO 0

enum a2p_action {
    A2P_ACTION_AUDIO,
    A2P_ACTION_Y4M,
    A2P_ACTION_PACKEDRAW,
    A2P_ACTION_INFO,
    A2P_ACTION_X264BD,
    A2P_ACTION_BENCHMARK,
    A2P_ACTION_NOTHING
};

static char short_opts[] = "a::Bb::ip::t::v::w::x::";
static struct option long_opts[] = {
    {"audio",       optional_argument, NULL, 'w'},
    {"rawaudio",    optional_argument, NULL, 'a'},
    {"y4mp",        optional_argument, NULL, 'p'},
    {"y4mt",        optional_argument, NULL, 't'},
    {"y4mb",        optional_argument, NULL, 'b'},
    {"packedraw",   optional_argument, NULL, 'v'},
    {"info",              no_argument, NULL, 'i'},
    {"x264bd",      optional_argument, NULL, 'x'},
    {"benchmark",         no_argument, NULL, 'B'},
    {0,0,0,0}
};

struct params {
    int sar_x;
    int sar_y;
    char ip;
    int is_raw;
    char *bit;
    enum a2p_action action;
};

AVS_Clip * avisynth_filter(AVS_Clip *clip, AVS_ScriptEnvironment *env, const char *filter);

AVS_Clip * avisynth_filter_yuv2yuv(AVS_Clip *clip, AVS_ScriptEnvironment *env,
                                   const char *filter, int interlaced);

AVS_Clip * avisynth_filter_rgb2yuv(AVS_Clip *clip, AVS_ScriptEnvironment *env,
                                   const char *filter, const char *matrix,
                                   int interlaced);

AVS_Clip * avisynth_source(char *file, AVS_ScriptEnvironment *env);

int64_t a2pm_gettime(void);

char * pix_type_to_string(int pix_type);

void do_audio(AVS_Clip *clip, AVS_ScriptEnvironment *env, struct params params);

void do_y4m(AVS_Clip *clip, AVS_ScriptEnvironment *env, struct params params);

void do_packedraw(AVS_Clip *clip, AVS_ScriptEnvironment *env);

void do_info(AVS_Clip *clip, AVS_ScriptEnvironment *env, char *input, int audio);

void do_benchmark(AVS_Clip *clip, AVS_ScriptEnvironment *env, char *input);

void do_x264bd(AVS_Clip *clip, AVS_ScriptEnvironment *env, struct params params);

struct params parse_opts(int argc, char **argv, struct params params);

void usage(char *binary);

#endif // AVS2PIPEMOD_H
