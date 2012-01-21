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
 * along with avs2pipe.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <io.h>
#include <fcntl.h>
#include <sys/timeb.h>
#include <time.h>

#include "avs2pipemod.h"
#include "wave.h"
#include "avs_func.h"

#define BM_FRAMES_PAR_OUT 50
#define Y4M_FRAME_HEADER "FRAME\n"
#define Y4M_FRAME_HEADER_SIZE 6

typedef enum {
    TYPE_INFO,
    TYPE_VIDEO_OUT,
    TYPE_Y4M_HEADER,
    TYPE_FILTER_26,
    TYPE_FILTER_25
} string_target_t;

static int64_t get_current_time(void)
{
    struct timeb tb;
    ftime( &tb );
    return ((int64_t)tb.time * 1000 + (int64_t)tb.millitm) * 1000;
}

static AVS_Value update_clip(avs_hnd_t *ah, AVS_Value res, AVS_Value release)
{
    if (!avs_is_error(res)) { /* If res is error, clip will be released at the last of main(). */
        ah->func.avs_release_clip(ah->clip);
        ah->clip = ah->func.avs_take_clip(res, ah->env);
        ah->vi = ah->func.avs_get_video_info(ah->clip);
    }
    ah->func.avs_release_value(release);
    return res;
}

static AVS_Value invoke_filter(avs_hnd_t *ah, AVS_Value res, const char *filter)
{
    AVS_Value tmp = ah->func.avs_invoke(ah->env, filter, res, NULL);
    a2pm_log(A2PM_LOG_INFO, "invoking %s() ...\n", filter);
    return update_clip(ah, tmp, res);
}

static AVS_Value invoke_trim(params_t *pr, avs_hnd_t *ah, AVS_Value res)
{
    AVS_Value arg_arr[3] = {res, avs_new_value_int(pr->trim[0]), avs_new_value_int(pr->trim[1])};
    a2pm_log(A2PM_LOG_INFO, "invoking Trim(%d,%d) ...\n", pr->trim[0], pr->trim[1]);
    AVS_Value tmp = ah->func.avs_invoke(ah->env, "Trim", avs_new_value_array(arg_arr, 3), NULL);
    return update_clip(ah, tmp, res);
}

static AVS_Value invoke_convert_csp(params_t *pr, avs_hnd_t *ah, AVS_Value res, const char *filter)
{
    const char *matrix = ah->vi->height < 720 ? "Rec601" : "Rec709";
    const char *arg_name[3] = {NULL, "interlaced", "matrix"};
    AVS_Value arg_arr[3] = {res, avs_new_value_bool(pr->frame_type != 'P'),
                            avs_new_value_string(matrix)};
    const int num_array = avs_is_yuy2(ah->vi) ? 2 : 3;
    if (avs_is_yuy2(ah->vi)) {
        a2pm_log(A2PM_LOG_INFO, "invoking %s(interlaced=%s) ...\n",
                 filter, pr->frame_type != 'p' ? "true" : "false");
    } else {
        a2pm_log(A2PM_LOG_INFO, "invoking %s(matrix=%s,interlaced=%s) ...\n",
                 filter, matrix, pr->frame_type != 'p' ? "true" : "false");
    }
    AVS_Value tmp = ah->func.avs_invoke(ah->env, filter, avs_new_value_array(arg_arr, num_array), arg_name);
    return update_clip(ah, tmp, res);
}

static char *get_string_from_pix(int input_pix_type, string_target_t type)
{
    static const struct {
        int pix_type;
        char *string_info;
        char *string_video_out;
        char *string_y4m_header;
        char *string_filter_26;
        char *string_filter_25;
    } pix_string_table[] = {
        {AVS_CS_BGR32, "RGB32", "BGRA",             NULL,       "ConvertToYV24", "ConvertToYV12"},
        {AVS_CS_BGR24, "RGB24", "BGR",              NULL,       "ConvertToYV24", "ConvertToYV12"},
        {AVS_CS_YUY2,  "YUY2",  "YUYV-422-packed",  NULL,       "ConvertToYV16", "ConvertToYV12"},
        {AVS_CS_YV12,  "YV12",  "YUV-420-planar",   "420mpeg2", NULL,            NULL           },
        {AVS_CS_I420,  "YV12",  "YUV-420-planar",   "420mpeg2", NULL,            NULL           },
        {AVS_CS_YV24,  "YV24",  "YUV-444-planar",   "444",      NULL,            NULL           },
        {AVS_CS_YV16,  "YV16",  "YUV-422-planar",   "422",      NULL,            NULL           },
        {AVS_CS_YV411, "YV411", "YUV-411-planar",   "411",      NULL,            NULL           },
        {AVS_CS_Y8,    "Y8",    "luma only (gray)", "mono",     NULL,            NULL           },
        {AVS_CS_UNKNOWN, NULL, NULL, NULL, NULL, NULL}
    };

    for (int i = 0; pix_string_table[i].pix_type != AVS_CS_UNKNOWN; i++)
        if (input_pix_type == pix_string_table[i].pix_type)
            switch (type) {
            case TYPE_INFO:
                return pix_string_table[i].string_info;
            case TYPE_VIDEO_OUT:
                return pix_string_table[i].string_video_out;
            case TYPE_Y4M_HEADER:
                return pix_string_table[i].string_y4m_header;
            case TYPE_FILTER_26:
                return pix_string_table[i].string_filter_26;
            case TYPE_FILTER_25:
                return pix_string_table[i].string_filter_25;
            default:
                break;
            }

    return "UNKNOWN";
}

static int get_chroma_shared_value(int pix_type, int is_horizontal)
{
    static const struct {
        int pix_type;
        int horizontal;
        int vertical;
    } value_table[] = {
        {AVS_CS_YUY2,    1, 0},
        {AVS_CS_YV12,    1, 1},
        {AVS_CS_I420,    1, 1},
        {AVS_CS_YV24,    0, 0},
        {AVS_CS_YV16,    1, 0},
        {AVS_CS_YV411,   2, 0},
        {AVS_CS_UNKNOWN, 0, 0}
    };
    for (int i = 0; value_table[i].pix_type != AVS_CS_UNKNOWN; i++)
        if (pix_type == value_table[i].pix_type)
            return is_horizontal ? value_table[i].horizontal : value_table[i].vertical;
    return 0;
}

static int write_packed_frames(avs_hnd_t *ah)
{
    int width = ah->vi->width * avs_bits_per_pixel(ah->vi) >> 3;
    size_t count = width * ah->vi->height;

    BYTE *buff = (BYTE*)malloc(count);
    RETURN_IF_ERROR(!buff, 0, "malloc failed.\n");

    int wrote;
    for (wrote = 0; wrote < ah->vi->num_frames; wrote++) {
        AVS_VideoFrame *frame = ah->func.avs_get_frame(ah->clip, wrote);
        ah->func.avs_bit_blt(ah->env, buff, width, avs_get_read_ptr(frame),
                             avs_get_pitch(frame), width, ah->vi->height);
        size_t step = fwrite(buff, sizeof(BYTE), count, stdout);
        ah->func.avs_release_video_frame(frame);
        if (step != count)
            break;
    }
    free(buff);
    return wrote;
}

static int write_planar_frames(params_t *pr, avs_hnd_t *ah)
{
    const int planes[] = {AVS_PLANAR_Y, AVS_PLANAR_U, AVS_PLANAR_V};
    const int num_planes = avs_is_y8(ah->vi) ? 1 : 3;

    int width[num_planes];
    int height[num_planes];
    int buff_inc[num_planes];
    for (int p = 0; p < num_planes; p++) {
        width[p] = ah->vi->width >> (p ? get_chroma_shared_value(ah->vi->pixel_type, 1) : 0);
        height[p] = ah->vi->height >> (p ? get_chroma_shared_value(ah->vi->pixel_type, 0) : 0);
        buff_inc[p] = width[p] * height[p];
    }

    size_t count = ((ah->vi->width * ah->vi->height * avs_bits_per_pixel(ah->vi)) >> 3)
                   + (pr->format_type == A2PM_FMT_YUV4MPEG2 ? Y4M_FRAME_HEADER_SIZE : 0);

    BYTE *buff = (BYTE*)malloc(count);
    RETURN_IF_ERROR(!buff, 0, "malloc failed.\n");

    BYTE *buff_0 = buff;
    if (pr->format_type == A2PM_FMT_YUV4MPEG2) {
        memcpy(buff, Y4M_FRAME_HEADER, Y4M_FRAME_HEADER_SIZE);
        buff_0 += Y4M_FRAME_HEADER_SIZE;
    }

    int wrote;
    for (wrote = 0; wrote < ah->vi->num_frames; wrote++) {
        AVS_VideoFrame *frame = ah->func.avs_get_frame(ah->clip, wrote);
        BYTE *buff_ptr = buff_0;
        for (int p = 0; p < num_planes; p++) {
            ah->func.avs_bit_blt(ah->env, buff_ptr, width[p], avs_get_read_ptr_p(frame, planes[p]),
                                 avs_get_pitch_p(frame, planes[p]), width[p], height[p]);
            buff_ptr += buff_inc[p];
        }
        size_t step = fwrite(buff, sizeof(BYTE), count, stdout);
        ah->func.avs_release_video_frame(frame);
        if (step != count)
            break;
    }
    free(buff);
    return wrote;
}

int act_do_video(params_t *pr, avs_hnd_t *ah, AVS_Value res)
{
    RETURN_IF_ERROR(!avs_has_video(ah->vi), -1, "clip has no video.\n");

    if (pr->trim[0] || pr->trim[1]) {
        res = invoke_trim(pr, ah, res);
        RETURN_IF_ERROR(avs_is_error(res), -1, "failed to invoke Trim.\n");
    }

    if (pr->format_type != A2PM_FMT_YUV4MPEG2)
        goto skip_rawvideo;

    if (avs_is_field_based(ah->vi)) {
        a2pm_log(A2PM_LOG_WARNING, "clip is FieldBased.\n"
                 "           yuv4mpeg2's spec doesn't support fieldbased clip.\n"
                 "           choose what you want.\n"
                 "           1:add AssumeFrameBased()  2:add Weave()  others:exit\n"
                 "               input a number and press enter : ");
        int k;
        fscanf(stdin, "%d", &k);
        switch (k) {
        case 1:
            res = invoke_filter(ah, res, "AssumeFrameBased");
            RETURN_IF_ERROR(avs_is_error(res), -1, "failed to invoke AssumeFrameBased()\n")
            break;
        case 2:
            res = invoke_filter(ah, res, "Weave");
            RETURN_IF_ERROR(avs_is_error(res), -1, "failed to invoke Weave()\n")
            break;
        default:
            RETURN_IF_ERROR(1, -1, "good bye...\n");
        }
    }

    if (!avs_is_planar(ah->vi)) {
        const char *convert = get_string_from_pix(ah->vi->pixel_type,
                                                  ah->version < 2.59 ? TYPE_FILTER_25 : TYPE_FILTER_26);
        res = invoke_convert_csp(pr, ah, res, convert);
        RETURN_IF_ERROR(avs_is_error(res), -1,
                        "failed to invoke %s. please check resolution.\n", convert);
    }

skip_rawvideo:
    if (pr->format_type == A2PM_FMT_RAWVIDEO_VFLIP) {
        res = invoke_filter(ah, res, "FlipVertical");
        RETURN_IF_ERROR(avs_is_error(res), -1, "failed to invoke FlipVertical()\n")
    }

    RETURN_IF_ERROR(_setmode(_fileno(stdout), _O_BINARY) == -1, -1,
                    "cannot switch stdout to binary mode.\n");

    if (pr->format_type == A2PM_FMT_YUV4MPEG2)
        a2pm_log(A2PM_LOG_INFO, "writing %d frames of %d/%d fps, %dx%d,\n"
                                "%20ssar %d:%d, %s %s video.\n",
                 ah->vi->num_frames, ah->vi->fps_numerator, ah->vi->fps_denominator,
                 ah->vi->width, ah->vi->height, "", pr->sar[0], pr->sar[1],
                 get_string_from_pix(ah->vi->pixel_type, TYPE_VIDEO_OUT),
                 pr->frame_type == 'p' ? "progressive" : pr->frame_type == 't' ? "tff" : "bff");
    else
        a2pm_log(A2PM_LOG_INFO, "writing %d frames of %dx%d %s rawvideo.\n",
                 ah->vi->num_frames, ah->vi->width, ah->vi->height,
                 get_string_from_pix(ah->vi->pixel_type, TYPE_VIDEO_OUT));

    int64_t start = get_current_time();

    if (pr->format_type == A2PM_FMT_YUV4MPEG2) {
        fprintf(stdout, "YUV4MPEG2 W%d H%d F%u:%u I%c A%d:%d C%s\n",
                ah->vi->width, ah->vi->height, ah->vi->fps_numerator, ah->vi->fps_denominator,
                pr->frame_type, pr->sar[0], pr->sar[1], get_string_from_pix(ah->vi->pixel_type, TYPE_Y4M_HEADER));
        fflush(stdout);
    }

    int wrote = avs_is_planar(ah->vi) ? write_planar_frames(pr, ah) :
                                        write_packed_frames(ah);

    fflush(stdout);

    int64_t elapsed = get_current_time() - start;

    a2pm_log(A2PM_LOG_REPEAT, "finished, wrote %d frames [%d%%].\n",
             wrote, (100 * wrote) / ah->vi->num_frames);

    a2pm_log(A2PM_LOG_INFO, "total elapsed time is %.3f sec [%.3ffps].\n",
             elapsed / 1000000.0, wrote * 1000000.0 / elapsed);

    RETURN_IF_ERROR(wrote != ah->vi->num_frames, -1, "only wrote %d of %d frames.\n", wrote, ah->vi->num_frames);

    return 0;
}

static inline void nope() {}

int act_do_audio(params_t *pr, avs_hnd_t *ah, AVS_Value res)
{
    RETURN_IF_ERROR(!avs_has_audio(ah->vi), -1, "clip has no audio.\n");

    if (avs_has_video(ah->vi) && (pr->trim[0] || pr->trim[1])) {
        res = invoke_trim(pr, ah, res);
        RETURN_IF_ERROR(avs_is_error(res), -1, "failed to invoke Trim.\n");
    }

    if (pr->bit) {
        char tmp[20];
        if (sprintf(tmp, "ConvertAudioTo%s", pr->bit) != EOF) {
            const char *filter = tmp;
            res = invoke_filter(ah, res, filter);
            RETURN_IF_ERROR(avs_is_error(res), -1, "failed to invoke %s()\n", filter);
        }
    }

    RETURN_IF_ERROR(_setmode(_fileno(stdout), _O_BINARY) == -1, -1,
                    "cannot switch stdout to binary mode.\n");

    size_t step = 0;
    size_t count = ah->vi->audio_samples_per_second;
    size_t size = avs_bytes_per_channel_sample(ah->vi) * ah->vi->nchannels;
    uint64_t target = ah->vi->num_audio_samples;

    void *buff = malloc(size * count);

    a2pm_log(A2PM_LOG_INFO, "writing %.3f seconds of %d Hz, %d channel audio.\n",
            (double)ah->vi->num_audio_samples / ah->vi->audio_samples_per_second,
            ah->vi->audio_samples_per_second, ah->vi->nchannels);

    int64_t start = get_current_time();

    if (pr->format_type == A2PM_FMT_RAWAUDIO)
        goto skip_rawaudio;

    WaveFormatType format = ah->vi->sample_type == AVS_SAMPLE_FLOAT ?
                            WAVE_FORMAT_IEEE_FLOAT : WAVE_FORMAT_PCM;
    wave_args_t args = {format, ah->vi->nchannels, ah->vi->audio_samples_per_second,
                        avs_bytes_per_channel_sample(ah->vi), ah->vi->num_audio_samples};

    switch (pr->format_type) {
#if 0
    case A2PM_FMT_RF64:
        WaveRf64Header *header = wave_create_rf64_header(&args);
        fwrite(header, sizeof(*header), 1, stdout);
        free(header);
        break;
#endif
    case A2PM_FMT_WAVEFORMATEXTENSIBLE:
        nope();
        WaveRiffExtHeader *header_ext = wave_create_riff_ext_header(&args);
        fwrite(header_ext, sizeof(*header_ext), 1, stdout);
        free(header_ext);
        break;
    case A2PM_FMT_WAVEFORMATEX:
        nope();
        WaveRiffHeader *header_wav = wave_create_riff_header(&args);
        fwrite(header_wav, sizeof(*header_wav), 1, stdout);
        free(header_wav);
        break;
    default:
        break;
    }

skip_rawaudio:
    ah->func.avs_get_audio(ah->clip, buff, 0, target % count);
    uint64_t wrote = fwrite(buff, size, target % count, stdout);
    while (wrote < target) {
        ah->func.avs_get_audio(ah->clip, buff, wrote, count);
        step = fwrite(buff, size, count, stdout);
        if (step != count)
            break;
        wrote += step;
    }

    fflush(stdout);

    a2pm_log(A2PM_LOG_REPEAT, "finished, wrote %.3f seconds [%I64u%%].\n",
            (double)wrote / ah->vi->audio_samples_per_second, (100 * wrote) / target);

    free(buff);

    int64_t elapsed = get_current_time() - start;
    a2pm_log(A2PM_LOG_INFO, "total elapsed time is %.3f sec.\n", elapsed / 1000000.0);

    RETURN_IF_ERROR(wrote != target, -1, "only wrote %I64u of %I64u samples.\n", wrote, target);

    return 0;
}

int act_do_info(params_t *pr, avs_hnd_t *ah)
{
    fprintf(stdout, "avisynth_version %.2f\n"
                    "script_name      %s\n",
                    ah->version, pr->input);
    if (avs_has_video(ah->vi)) {
        fprintf(stdout,
                "v:width          %d\n"
                "v:height         %d\n"
                "v:fps            %d/%d\n"
                "v:frames         %d\n"
                "v:duration[sec]  %.3f\n"
                "v:image_type     %s\n",
                ah->vi->width, ah->vi->height, ah->vi->fps_numerator, ah->vi->fps_denominator,
                ah->vi->num_frames, (double)ah->vi->num_frames * ah->vi->fps_denominator / ah->vi->fps_numerator,
                !avs_is_field_based(ah->vi) ? "framebased" : "fieldbased");

        char *field_order;
        switch (ah->vi->image_type) {
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
        fprintf(stdout,
                "v:field_order    %s\n"
                "v:pixel_type     %s\n",
                field_order, get_string_from_pix(ah->vi->pixel_type, TYPE_INFO));
    }

    if (avs_has_audio(ah->vi) && pr->action == A2PM_ACT_INFO) {
        fprintf(stdout,
                "a:sample_rate    %d\n"
                "a:format         %s\n"
                "a:bit_depth      %d\n"
                "a:channels       %d\n"
                "a:samples        %I64d\n"
                "a:duration[sec]  %.3f\n",
                ah->vi->audio_samples_per_second,
                ah->vi->sample_type == AVS_SAMPLE_FLOAT ? "float" : "pcm",
                avs_bytes_per_channel_sample(ah->vi) * 8, ah->vi->nchannels,
                ah->vi->num_audio_samples,
                (double)ah->vi->num_audio_samples / ah->vi->audio_samples_per_second);
    }
    return 0;
}

int act_do_benchmark(params_t *pr, avs_hnd_t *ah, AVS_Value res)
{
    RETURN_IF_ERROR(!avs_has_video(ah->vi), -1, "clip has no video.\n");

    if (pr->trim[0] || pr->trim[1]) {
        res = invoke_trim(pr, ah, res);
        RETURN_IF_ERROR(avs_is_error(res), -1, "failed to invoke Trim.\n");
    }

    act_do_info(pr, ah);

    AVS_VideoFrame *frame;
    int surplus = ah->vi->num_frames % BM_FRAMES_PAR_OUT;
    int passed = 0;
    int64_t elapsed;

    a2pm_log(A2PM_LOG_INFO, "benchmarking %d frames video.\n", ah->vi->num_frames);

    int64_t start = get_current_time();

    while (passed < surplus) {
        frame = ah->func.avs_get_frame(ah->clip, passed);
        ah->func.avs_release_video_frame(frame);
        passed++;
    }

    while (passed < ah->vi->num_frames) {
        elapsed = get_current_time() - start;
        a2pm_log(A2PM_LOG_REPEAT, "[elapsed %.3f sec] %d/%d frames [%3d%%][%.3ffps]",
                 elapsed / 1000000.0, passed, ah->vi->num_frames,
                 passed * 100 / ah->vi->num_frames, passed * 1000000.0 / elapsed);
        for (int f = 0; f < BM_FRAMES_PAR_OUT; f++) {
            frame = ah->func.avs_get_frame(ah->clip, passed);
            ah->func.avs_release_video_frame(frame);
            passed++;
        }
    }
    elapsed = get_current_time() - start;
    fprintf(stderr, "\n");
    fprintf(stdout, "benchmark result: total elapsed time is %.3f sec [%.3ffps]\n",
            elapsed / 1000000.0, passed * 1000000.0 / elapsed);
    RETURN_IF_ERROR(passed != ah->vi->num_frames, -1, "only passed %d of %d frames.\n", passed, ah->vi->num_frames);

    return 0;
}

int act_do_x264bd(params_t *pr, avs_hnd_t *ah)
{
    RETURN_IF_ERROR(ah->vi->height != 480 && ah->vi->height != 576 && pr->is_sdbd, -1,
                    "DAR 4:3 is supported only NTSC/PAL SD source\n");

    const struct {
        int width;
        int height;
        int fpsnum;
        int fpsden;
        char frame_type;
        int keyint;
        char *flag;
        int sar_x;
        int sar_y;
        char* color;
    } bd_spec_table[] = {
        /* 1080p24 */
        {1920, 1080, 24000, 1001, 'p', 24, ""                 ,  1,  1,     "bt709"},
        {1920, 1080,    24,    1, 'p', 24, ""                 ,  1,  1,     "bt709"},
        {1920, 1080,  2997,  125, 'p', 24, ""                 ,  1,  1,     "bt709"},
        {1440, 1080, 24000, 1001, 'p', 24, ""                 ,  4,  3,     "bt709"},
        {1440, 1080,    24,    1, 'p', 24, ""                 ,  4,  3,     "bt709"},
        {1440, 1080,  2997,  125, 'p', 24, ""                 ,  4,  3,     "bt709"},
        /* 1080p25 */
        {1920, 1080,    25,    1, 'p', 25, "--fake-interlaced",  1,  1,     "bt709"},
        {1440, 1080,    25,    1, 'p', 25, "--fake-interlaced",  4,  3,     "bt709"},
        /* 1080p30 */
        {1920, 1080, 30000, 1001, 'p', 30, "--fake-interlaced",  1,  1,     "bt709"},
        {1920, 1080,  2997,  100, 'p', 30, "--fake-interlaced",  1,  1,     "bt709"},
        {1440, 1080, 30000, 1001, 'p', 30, "--fake-interlaced",  4,  3,     "bt709"},
        {1440, 1080,  2997,  100, 'p', 30, "--fake-interlaced",  4,  3,     "bt709"},
        /* 1080i25*/
        {1920, 1080,    25,    1, 't', 25, "--tff"            ,  1,  1,     "bt709"},
        {1440, 1080,    25,    1, 't', 25, "--tff"            ,  4,  3,     "bt709"},
        /* 1080i30 */
        {1920, 1080, 30000, 1001, 't', 30, "--tff"            ,  1,  1,     "bt709"},
        {1920, 1080,  2997,  100, 't', 30, "--tff"            ,  1,  1,     "bt709"},
        {1920, 1080,    30,    1, 't', 30, "--tff"            ,  1,  1,     "bt709"},
        {1440, 1080, 30000, 1001, 't', 30, "--tff"            ,  4,  3,     "bt709"},
        {1440, 1080,  2997,  100, 't', 30, "--tff"            ,  4,  3,     "bt709"},
        {1440, 1080,    30,    1, 't', 30, "--tff"            ,  4,  3,     "bt709"},
        /* 720p24 */
        {1280,  720, 24000, 1001, 'p', 24, ""                 ,  1,  1,     "bt709"},
        {1280,  720,    24,    1, 'p', 24, ""                 ,  1,  1,     "bt709"},
        {1280,  720,  2997,  125, 'p', 24, ""                 ,  1,  1,     "bt709"},
        /* 720p25 */
        {1280,  720,    25,    1, 'p', 25, "--pulldown double",  1,  1,     "bt709"},
        /* 720p30 */
        {1280,  720, 30000, 1001, 'p', 25, "--pulldown double",  1,  1,     "bt709"},
        /* 720p50 */
        {1280,  720,    50,    1, 'p', 50, ""                 ,  1,  1,     "bt709"},
        /* 720p60 */
        {1280,  720, 60000, 1001, 'p', 60, ""                 ,  1,  1,     "bt709"},
        {1280,  720,  2997,   50, 'p', 60, ""                 ,  1,  1,     "bt709"},
        /* 576p25 */
        { 720,  576,    25,    1, 'p', 25, "--fake-interlaced", 16, 11,   "bt470bg"},
        /* 576i25 */
        { 720,  576,    25,    1, 't', 25, "--tff"            , 16, 11,   "bt470bg"},
        /* 480p24 */
        { 720,  480, 24000, 1001, 'p', 24, "--pulldown 32"    , 40, 33, "smpte170m"},
        /* 480i30*/
        { 720,  480, 30000, 1001, 't', 30, "--tff"            , 40, 33, "smpte170m"},
        {0}
    };

    int keyint  = 0;
    char *flag  = NULL;
    char *color = NULL;
    for (int i = 0; bd_spec_table[i].width != 0; i++) {
        if (ah->vi->width == bd_spec_table[i].width
         && ah->vi->height == bd_spec_table[i].height
         && ah->vi->fps_numerator == bd_spec_table[i].fpsnum
         && ah->vi->fps_denominator == bd_spec_table[i].fpsden
         && pr->frame_type == bd_spec_table[i].frame_type) {
            keyint     = bd_spec_table[i].keyint;
            flag       = bd_spec_table[i].flag;
            color      = bd_spec_table[i].color;
            pr->sar[0] = bd_spec_table[i].sar_x;
            pr->sar[1] = bd_spec_table[i].sar_y;
        }
    }
    RETURN_IF_ERROR(!keyint, -1, "%dx%d @ %d/%d fps not supported.\n",
                    ah->vi->width, ah->vi->height, ah->vi->fps_numerator, ah->vi->fps_denominator);

    if (pr->is_sdbd) {
        pr->sar[0] = ah->vi->height == 576 ? 12 : 10;
        pr->sar[1] = 11;
    }

    fprintf(stdout,
            " --bluray-compat"
            " --vbv-maxrate 40000"
            " --vbv-bufsize 30000"
            " --level 4.1"
            " --keyint %d"
            " --open-gop"
            " --slices 4"
            " %s"
            " --sar %d:%d"
            " --colorprim %s"
            " --transfer %s"
            " --colormatrix %s",
            keyint, flag, pr->sar[0], pr->sar[1], color, color, color);
    return 0;
}