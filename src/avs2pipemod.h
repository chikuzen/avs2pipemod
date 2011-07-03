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

#define BM_FRAMES_PAR_OUT       50
#define Y4M_FRAME_HEADER_STRING "FRAME\n"
#define Y4M_FRAME_HEADER_SIZE   6
#define BUFSIZE_OF_STDOUT       262144 // 256KiB. The optimum value might be smaller than this.
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

const AVS_VideoInfo *
avisynth_filter(AVS_Clip *clip, AVS_ScriptEnvironment *env, const AVS_VideoInfo *info,
                const char *filter);

const AVS_VideoInfo *
avisynth_filter_yuv2yuv(AVS_Clip *clip, AVS_ScriptEnvironment *env, const AVS_VideoInfo *info,
                        const char *filter, a2pm_bool is_interlaced);

const AVS_VideoInfo *
avisynth_filter_rgb2yuv(AVS_Clip *clip, AVS_ScriptEnvironment *env, const AVS_VideoInfo *info,
                        const char *filter, const char *matrix, a2pm_bool is_interlaced);

AVS_Clip * avisynth_source(char *file, AVS_ScriptEnvironment *env);

int64_t a2pm_gettime(void);

char * pix_type_to_string(int pix_type, a2pm_bool is_info);

void do_audio(AVS_Clip *clip, AVS_ScriptEnvironment *env, params *params);

void do_y4m(AVS_Clip *clip, AVS_ScriptEnvironment *env, params *params);

void do_rawvideo(AVS_Clip *clip, AVS_ScriptEnvironment *env, params *params);

void do_info(AVS_Clip *clip, AVS_ScriptEnvironment *env, char *input, a2pm_bool need_audio);

void do_benchmark(AVS_Clip *clip, AVS_ScriptEnvironment *env, char *input);

void do_x264bd(AVS_Clip *clip, AVS_ScriptEnvironment *env, params *params);

void parse_opts(int argc, char **argv, params *params);

#ifdef A2P_AVS26
void a2pm_csp_handle26(int pix_type, yuvout *data);
#endif

int a2pm_write_planar_frames(AVS_Clip *clip, const AVS_VideoInfo *info, yuvout *yuvout, a2pm_bool is_raw);

int a2pm_write_packed_frames(AVS_Clip *clip, const AVS_VideoInfo *info);

void usage(char *binary);

#endif // AVS2PIPEMOD_H
