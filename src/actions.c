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
#include <stdlib.h>
#include <io.h>
#include <fcntl.h>
#include <sys/timeb.h>
#include <time.h>

#include "avs2pipemod.h"
#include "wave.h"
#include "avs_func.h"
#include "actions.h"

#define BM_FRAMES_PAR_OUT 50
#define Y4M_FRAME_HEADER "FRAME\n"
#define Y4M_FRAME_HEADER_SIZE 6

#define PASS_OR_TRIM \
    if (pr->trim[0] || pr->trim[1]) { \
        res = invoke_trim(pr, ah, res); \
        RETURN_IF_ERROR(avs_is_error(res), -1, "failed to invoke Trim.\n"); \
    }

typedef enum {
    TYPE_INFO,
    TYPE_VIDEO_OUT,
    TYPE_Y4M_HEADER,
    TYPE_FILTER_26,
    TYPE_FILTER_25,
    TYPE_X264_DEMUXER,
    TYPE_X264_INPUT_CSP,
    TYPE_X264_OUTPUT_CSP
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
    if (avs_is_yuy2(ah->vi))
        a2pm_log(A2PM_LOG_INFO, "invoking %s(interlaced=%s) ...\n",
                 filter, pr->frame_type != 'p' ? "true" : "false");
    else
        a2pm_log(A2PM_LOG_INFO, "invoking %s(matrix=%s,interlaced=%s) ...\n",
                 filter, matrix, pr->frame_type != 'p' ? "true" : "false");
    AVS_Value tmp = ah->func.avs_invoke(ah->env, filter,
                                        avs_new_value_array(arg_arr, avs_is_yuy2(ah->vi) ? 2 : 3), arg_name);
    return update_clip(ah, tmp, res);
}

static char *get_string_from_pix(int input_pix_type, string_target_t type)
{
    struct {
        int pix_type;
        char *info;
        char *video_out;
        char *y4m_header;
        char *filter_26;
        char *filter_25;
        char *x264_demuxer;
        char *x264_input_csp;
        char *x264_output_csp;
    } pix_string_table[] = {
        {AVS_CS_BGR32  , "RGB32", "BGRA"            , NULL      , "ConvertToYV24", "ConvertToYV12", "raw"                      , "bgra"   , "rgb" },
        {AVS_CS_BGR24  , "RGB24", "BGR"             , NULL      , "ConvertToYV24", "ConvertToYV12", "raw"                      , "bgr"    , "rgb" },
        {AVS_CS_YUY2   , "YUY2" , "YUYV-422-packed" , NULL      , "ConvertToYV16", "ConvertToYV12", "lavf --input-fmt rawvideo", "yuyv422", "i422"},
        {AVS_CS_YV12   , "YV12" , "YUV-420-planar"  , "420mpeg2", NULL           , NULL           , "raw"                      , "i420"   , "i420"},
        {AVS_CS_I420   , "YV12" , "YUV-420-planar"  , "420mpeg2", NULL           , NULL           , "raw"                      , "i420"   , "i420"},
        {AVS_CS_YV24   , "YV24" , "YUV-444-planar"  , "444"     , NULL           , NULL           , "raw"                      , "i444"   , "i444"},
        {AVS_CS_YV16   , "YV16" , "YUV-422-planar"  , "422"     , NULL           , NULL           , "raw"                      , "i422"   , "i422"},
        {AVS_CS_YV411  , "YV411", "YUV-411-planar"  , "411"     , NULL           , NULL           , "lavf --input-fmt rawvideo", "yuv411p", "i422"},
        {AVS_CS_Y8     , "Y8"   , "luma only (gray)", "mono"    , NULL           , NULL           , "lavf --input-fmt rawvideo", "gray"   , "i420"},
        {input_pix_type , NULL}
    };

    char *string;
    int i = 0;
    while (pix_string_table[i].pix_type != input_pix_type)
        i++;

    switch (type) {
    case TYPE_INFO:
        string = pix_string_table[i].info;
        break;
    case TYPE_VIDEO_OUT:
        string = pix_string_table[i].video_out;
        break;
    case TYPE_Y4M_HEADER:
        string = pix_string_table[i].y4m_header;
        break;
    case TYPE_FILTER_26:
        string = pix_string_table[i].filter_26;
        break;
    case TYPE_FILTER_25:
        string = pix_string_table[i].filter_25;
        break;
    case TYPE_X264_DEMUXER:
        string = pix_string_table[i].x264_demuxer;
        break;
    case TYPE_X264_INPUT_CSP:
        string = pix_string_table[i].x264_input_csp;
        break;
    case TYPE_X264_OUTPUT_CSP:
        string = pix_string_table[i].x264_output_csp;
        break;
    default:
        string = "UNKNOWN";
    }

    return string;
}

static int write_riff_ext_header(wave_args_t *args)
{
    WaveRiffExtHeader *header = wave_create_riff_ext_header(args);
    RETURN_IF_ERROR(!header, -1, "failed to execute %s.\n", __func__);
    fwrite(header, sizeof(*header), 1, stdout);
    free(header);
    return 0;
}

static int write_riff_header(wave_args_t *args)
{
    WaveRiffHeader *header = wave_create_riff_header(args);
    RETURN_IF_ERROR(!header, -1, "failed to execute %s.\n", __func__);
    fwrite(header, sizeof(*header), 1, stdout);
    free(header);
    return 0;
}

static int write_audio_file_header(params_t *pr, avs_hnd_t *ah)
{
    WaveFormatType format = ah->vi->sample_type == AVS_SAMPLE_FLOAT ?
                            WAVE_FORMAT_IEEE_FLOAT : WAVE_FORMAT_PCM;
    wave_args_t args = {format, ah->vi->nchannels, ah->vi->audio_samples_per_second,
                        avs_bytes_per_channel_sample(ah->vi), ah->vi->num_audio_samples};

    switch (pr->format_type) {
    case A2PM_FMT_WAVEFORMATEXTENSIBLE:
        return write_riff_ext_header(&args);
    case A2PM_FMT_WAVEFORMATEX:
        return write_riff_header(&args);
    default:
        break;
    }
    return -1;
}

static int get_chroma_shared_value(int pix_type, int is_horizontal)
{
    struct {
        int pix_type;
        int horizontal;
        int vertical;
    } value_table[] = {
        {AVS_CS_YUY2   , 1, 0},
        {AVS_CS_YV12   , 1, 1},
        {AVS_CS_I420   , 1, 1},
        {AVS_CS_YV24   , 0, 0},
        {AVS_CS_YV16   , 1, 0},
        {AVS_CS_YV411  , 2, 0},
        {pix_type      , 0, 0}
    };

    int i = 0;
    while (value_table[i].pix_type != pix_type)
        i++;

    return is_horizontal ? value_table[i].horizontal : value_table[i].vertical;
}

static int write_packed_frames(avs_hnd_t *ah)
{
    int width = ah->vi->width * avs_bits_per_pixel(ah->vi) >> 3;
    size_t count = width * ah->vi->height;

    BYTE *buff = (BYTE*)malloc(count);
    RETURN_IF_ERROR(!buff, 0, "malloc failed at %s.\n", __func__);

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

static int write_packed_pix_values(avs_hnd_t *ah)
{
    int width = ah->vi->width * avs_bits_per_pixel(ah->vi) >> 3;
    int count = width * ah->vi->height;

    fprintf(stdout, "pixel order: %s\n\n", avs_is_rgb32(ah->vi) ? "BGRA" :
                                           avs_is_rgb24(ah->vi) ? "BGR" :
                                                                  "YUYV");
    int wrote = 0;
    while (wrote < ah->vi->num_frames) {
        AVS_VideoFrame *frame = ah->func.avs_get_frame(ah->clip, wrote);
        const char *err = ah->func.avs_clip_get_error(ah->clip);
        RETURN_IF_ERROR(err, wrote, "%s occurred while reading frame %d\n", err, wrote);
        fprintf(stdout, "frame %d\n", wrote);
        int step = 0;
        const BYTE *pix_value = avs_get_read_ptr(frame);
        for (int y = 0; y < ah->vi->height; y++) {
            for (int x = 0; x < width; x++, step++)
                fprintf(stdout, "%d\t", *(pix_value + x));
            fputc('\n', stdout);
            pix_value += avs_get_pitch(frame);
        }
        fputc('\n', stdout);
        ah->func.avs_release_video_frame(frame);
        if (step != count)
            break;
        wrote++;
    }
    return wrote;
}

#define NUM_PLANES 3
typedef struct {
    int planes[NUM_PLANES];
    int num_planes;
    int width[NUM_PLANES];
    int height[NUM_PLANES];
    int buff_inc[NUM_PLANES];
} planar_properties;
#undef NUM_PLANES

static planar_properties *get_planar_properties(avs_hnd_t *ah)
{
    planar_properties *properties = malloc(sizeof(*properties));
    RETURN_IF_ERROR(!properties, NULL, "malloc failed at %s.\n", __func__);
    properties->planes[0] = AVS_PLANAR_Y;
    properties->planes[1] = AVS_PLANAR_U;
    properties->planes[2] = AVS_PLANAR_V;
    properties->num_planes = avs_is_y8(ah->vi) ? 1 : 3;
    for (int p = 0; p < properties->num_planes; p++) {
        properties->width[p] = ah->vi->width >> (p ? get_chroma_shared_value(ah->vi->pixel_type, 1) : 0);
        properties->height[p] = ah->vi->height >> (p ? get_chroma_shared_value(ah->vi->pixel_type, 0) : 0);
        properties->buff_inc[p] = properties->width[p] * properties->height[p];
    }
    return properties;
}

static int write_planar_frames(params_t *pr, avs_hnd_t *ah)
{
    size_t count = ((ah->vi->width * ah->vi->height * avs_bits_per_pixel(ah->vi)) >> 3)
                   + (pr->format_type == A2PM_FMT_YUV4MPEG2 ? Y4M_FRAME_HEADER_SIZE : 0);

    BYTE *buff = (BYTE*)malloc(count);
    RETURN_IF_ERROR(!buff, 0, "malloc failed at %s.\n", __func__);

    BYTE *buff_0 = buff;
    if (pr->format_type == A2PM_FMT_YUV4MPEG2) {
        memcpy(buff, Y4M_FRAME_HEADER, Y4M_FRAME_HEADER_SIZE);
        buff_0 += Y4M_FRAME_HEADER_SIZE;
    }

    planar_properties *pp = get_planar_properties(ah);
    if (!pp) {
        free(buff);
        return 0;
    }

    int wrote = 0;
    while (wrote < ah->vi->num_frames) {
        AVS_VideoFrame *frame = ah->func.avs_get_frame(ah->clip, wrote);
        BYTE *buff_ptr = buff_0;
        for (int p = 0; p < pp->num_planes; p++) {
            ah->func.avs_bit_blt(ah->env, buff_ptr, pp->width[p], avs_get_read_ptr_p(frame, pp->planes[p]),
                                 avs_get_pitch_p(frame, pp->planes[p]), pp->width[p], pp->height[p]);
            buff_ptr += pp->buff_inc[p];
        }
        size_t step = fwrite(buff, sizeof(BYTE), count, stdout);
        ah->func.avs_release_video_frame(frame);
        if (step != count)
            break;
        wrote++;
    }

    free(pp);
    free(buff);
    return wrote;
}

static int write_planar_pix_values(avs_hnd_t *ah)
{
    int count = (ah->vi->width * ah->vi->height * avs_bits_per_pixel(ah->vi)) >> 3;
    planar_properties *pp = get_planar_properties(ah);
    if (!pp)
        return 0;

    int wrote = 0;
    while (wrote < ah->vi->num_frames) {
        AVS_VideoFrame *frame = ah->func.avs_get_frame(ah->clip, wrote);
        const char *err = ah->func.avs_clip_get_error(ah->clip);
        if (err) {
            a2pm_log(A2PM_LOG_ERROR, "%s occurred while reading frame %d\n", err, wrote);
            break;
        }

        fprintf(stdout, "frame %d\n", wrote);
        int step = 0;
        for (int p = 0; p < pp->num_planes; p++) {
            const BYTE* pix_value = avs_get_read_ptr_p(frame, pp->planes[p]);
            fprintf(stdout, "\nplane %s\n", !p ? "Y" : p == 1 ? "U" : "V");
            for (int y = 0; y < pp->height[p]; y++) {
                for (int x = 0; x < pp->width[p]; x++, step++)
                    fprintf(stdout, "%d\t", *(pix_value + x));
                fputc('\n', stdout);
                pix_value += avs_get_pitch_p(frame, pp->planes[p]);
            }
        }

        ah->func.avs_release_video_frame(frame);
        fputc('\n', stdout);
        if (step != count)
            break;
        wrote++;
    }

    free(pp);
    return wrote;
}

static int preprocess_for_y4mout(params_t *pr, avs_hnd_t *ah, AVS_Value res)
{
    if (avs_is_field_based(ah->vi)) {
        a2pm_log(A2PM_LOG_WARNING,
                 "clip is FieldBased.\n"
                 "%19s yuv4mpeg2's spec doesn't support fieldbased clip.\n"
                 "%19s choose what you want.\n"
                 "%19s 1:add AssumeFrameBased()  2:add Weave()  others:exit\n"
                 "%19s input a number and press enter : ",
                 "", "", "", "");
        int k = fgetc(stdin);
        switch (k) {
        case '1':
            res = invoke_filter(ah, res, "AssumeFrameBased");
            RETURN_IF_ERROR(avs_is_error(res), -1, "failed to invoke AssumeFrameBased()\n")
            break;
        case '2':
            res = invoke_filter(ah, res, "Weave");
            RETURN_IF_ERROR(avs_is_error(res), -1, "failed to invoke Weave()\n")
            break;
        default:
            RETURN_IF_ERROR(1, -1, "good bye...\n");
        }
    }

    if (!avs_is_planar(ah->vi)) {
        const char *convert
            = get_string_from_pix(ah->vi->pixel_type,
                                  (ah->version + 0.005) * 100 < 260 ?
                                  TYPE_FILTER_25 : TYPE_FILTER_26);
        res = invoke_convert_csp(pr, ah, res, convert);
        RETURN_IF_ERROR(avs_is_error(res), -1,
                        "failed to invoke %s. please check resolution.\n", convert);
    }

    return 0;
}

int act_do_video(params_t *pr, avs_hnd_t *ah, AVS_Value res)
{
    RETURN_IF_ERROR(!avs_has_video(ah->vi), -1, "clip has no video.\n");

    PASS_OR_TRIM;

    if (pr->format_type == A2PM_FMT_YUV4MPEG2 && preprocess_for_y4mout(pr, ah, res))
        return -1;

    if (pr->format_type == A2PM_FMT_RAWVIDEO_VFLIP) {
        res = invoke_filter(ah, res, "FlipVertical");
        RETURN_IF_ERROR(avs_is_error(res), -1, "failed to invoke FlipVertical()\n")
    }

    RETURN_IF_ERROR(_setmode(_fileno(stdout), _O_BINARY) == -1, -1,
                    "cannot switch stdout to binary mode.\n");

#define MESSAGE_LEN_MAX 128
    char *start_message = (char *)malloc(MESSAGE_LEN_MAX);
    RETURN_IF_ERROR(!start_message, -1, "malloc failed at %s.\n", __func__);
    switch (pr->format_type) {
    case A2PM_FMT_YUV4MPEG2:
        snprintf(start_message, MESSAGE_LEN_MAX,
                "writing %d frames of %d/%d fps, %dx%d,\n"
                "%19s sar %d:%d, %s %s video.\n",
                ah->vi->num_frames, ah->vi->fps_numerator, ah->vi->fps_denominator,
                ah->vi->width, ah->vi->height, "", pr->sar[0], pr->sar[1],
                get_string_from_pix(ah->vi->pixel_type, TYPE_VIDEO_OUT),
                pr->frame_type == 'p' ? "progressive" : pr->frame_type == 't' ? "tff" : "bff");
        break;
    default:
        snprintf(start_message, MESSAGE_LEN_MAX,
                "writing %d frames of %dx%d %s rawvideo.\n",
                ah->vi->num_frames, ah->vi->width, ah->vi->height,
                get_string_from_pix(ah->vi->pixel_type, TYPE_VIDEO_OUT));
        break;
    }
    a2pm_log(A2PM_LOG_INFO, "%s", start_message);
    free(start_message);
#undef MESSAGE_LEN_MAX

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

    a2pm_log(A2PM_LOG_INFO, "finished, wrote %d frames [%d%%].\n",
             wrote, (100 * wrote) / ah->vi->num_frames);

    a2pm_log(A2PM_LOG_INFO, "total elapsed time is %.3f sec [%.3ffps].\n",
             elapsed / 1000000.0, wrote * 1000000.0 / elapsed);

    RETURN_IF_ERROR(wrote != ah->vi->num_frames, -1, "only wrote %d of %d frames.\n", wrote, ah->vi->num_frames);

    return 0;
}

int act_do_audio(params_t *pr, avs_hnd_t *ah, AVS_Value res)
{
    RETURN_IF_ERROR(!avs_has_audio(ah->vi), -1, "clip has no audio.\n");

    if (avs_has_video(ah->vi))
        PASS_OR_TRIM;

    if (pr->bit) {
        char tmp[20];
        if (sprintf(tmp, "ConvertAudioTo%s", pr->bit) != EOF) {
            const char *filter = tmp;
            res = invoke_filter(ah, res, filter);
            RETURN_IF_ERROR(avs_is_error(res), -1, "failed to invoke %s()\n", filter);
        }
    }

    size_t step = 0;
    size_t count = ah->vi->audio_samples_per_second;
    size_t size = avs_bytes_per_channel_sample(ah->vi) * ah->vi->nchannels;
    uint64_t target = ah->vi->num_audio_samples;

    a2pm_log(A2PM_LOG_INFO, "writing %.3f seconds of %d Hz, %d channel audio.\n",
            (double)ah->vi->num_audio_samples / ah->vi->audio_samples_per_second,
            ah->vi->audio_samples_per_second, ah->vi->nchannels);

    RETURN_IF_ERROR(_setmode(_fileno(stdout), _O_BINARY) == -1, -1,
                    "cannot switch stdout to binary mode.\n");

    int64_t start = get_current_time();

    RETURN_IF_ERROR(pr->format_type != A2PM_FMT_RAWAUDIO && write_audio_file_header(pr, ah),
                    -1, "failed to write file header.\n");

    void *buff = malloc(size * count);
    RETURN_IF_ERROR(!buff, -1, "malloc failed at %s.\n", __func__);

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
    fprintf(stdout,
            "avisynth_version %.2f\n"
            "script_name      %s\n",
            ah->version, pr->input);

    if (avs_has_video(ah->vi)) {
        int fieldorder = avs_get_field_order(ah->vi);
        fprintf(stdout,
                "v:width          %d\n"
                "v:height         %d\n"
                "v:fps            %d/%d\n"
                "v:frames         %d\n"
                "v:duration[sec]  %.3f\n"
                "v:image_type     %s\n"
                "v:field_order    %s\n"
                "v:pixel_type     %s\n",
                ah->vi->width,
                ah->vi->height,
                ah->vi->fps_numerator, ah->vi->fps_denominator,
                ah->vi->num_frames,
                (double)ah->vi->num_frames * ah->vi->fps_denominator / ah->vi->fps_numerator,
                !avs_is_field_based(ah->vi) ? "framebased" : "fieldbased",
                fieldorder == AVS_IT_BFF ? "assumed bottom field first" :
                    fieldorder == AVS_IT_TFF ? "assumed top field first" :
                                               "not specified",
                get_string_from_pix(ah->vi->pixel_type, TYPE_INFO));
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
                avs_bytes_per_channel_sample(ah->vi) * 8,
                ah->vi->nchannels,
                ah->vi->num_audio_samples,
                (double)ah->vi->num_audio_samples / ah->vi->audio_samples_per_second);
    }
    return 0;
}

int act_do_benchmark(params_t *pr, avs_hnd_t *ah, AVS_Value res)
{
    RETURN_IF_ERROR(!avs_has_video(ah->vi), -1, "clip has no video.\n");

    PASS_OR_TRIM;

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
        fprintf(stderr, "\ravs2pipemod[info]: [elapsed %.3f sec] %d/%d frames [%3d%%][%.3ffps]",
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

int act_do_x264bd(params_t *pr, avs_hnd_t *ah, AVS_Value res)
{
    RETURN_IF_ERROR(!avs_has_video(ah->vi), -1, "clip has no video.\n");
    RETURN_IF_ERROR(ah->vi->width > 1920 || ah->vi->height > 1080, -1,
                    "clip has unsupported resolution.\n");
    RETURN_IF_ERROR(ah->vi->height != 480 && ah->vi->height != 576 && pr->format_type == A2PM_FMT_SDBD, -1,
                    "DAR 4:3 is supported only NTSC/PAL SD source\n");

#define WHNDT(width,height,fpsnum,fpsden,ftype) \
    ((uint64_t)(width) << 48 | (uint64_t)(height) << 36 | \
     (uint64_t)(fpsnum) << 20 | (uint64_t)(fpsden) << 8 | (uint64_t)(ftype))

    uint64_t src_res_fps_type
        = WHNDT(ah->vi->width, ah->vi->height,
                ah->vi->fps_numerator, ah->vi->fps_denominator, pr->frame_type);

    struct {
        uint64_t res_fps_type;
        int keyint;
        char *flag;
        int sar_x;
        int sar_y;
    } bd_spec_table[] = {
        /* 1080p24 */
        {WHNDT(1920, 1080, 24000, 1001, 'p'), 24, ""                 ,  1,  1},
        {WHNDT(1920, 1080,    24,    1, 'p'), 24, ""                 ,  1,  1},
        {WHNDT(1920, 1080,  2997,  125, 'p'), 24, ""                 ,  1,  1},
        {WHNDT(1440, 1080, 24000, 1001, 'p'), 24, ""                 ,  4,  3},
        {WHNDT(1440, 1080,    24,    1, 'p'), 24, ""                 ,  4,  3},
        {WHNDT(1440, 1080,  2997,  125, 'p'), 24, ""                 ,  4,  3},
        /* 1080p25 */
        {WHNDT(1920, 1080,    25,    1, 'p'), 25, "--fake-interlaced",  1,  1},
        {WHNDT(1440, 1080,    25,    1, 'p'), 25, "--fake-interlaced",  4,  3},
        /* 1080p30 */
        {WHNDT(1920, 1080, 30000, 1001, 'p'), 30, "--fake-interlaced",  1,  1},
        {WHNDT(1920, 1080,  2997,  100, 'p'), 30, "--fake-interlaced",  1,  1},
        {WHNDT(1440, 1080, 30000, 1001, 'p'), 30, "--fake-interlaced",  4,  3},
        {WHNDT(1440, 1080,  2997,  100, 'p'), 30, "--fake-interlaced",  4,  3},
        /* 1080i25*/
        {WHNDT(1920, 1080,    25,    1, 't'), 25, "--tff"            ,  1,  1},
        {WHNDT(1440, 1080,    25,    1, 't'), 25, "--tff"            ,  4,  3},
        /* 1080i30 */
        {WHNDT(1920, 1080, 30000, 1001, 't'), 30, "--tff"            ,  1,  1},
        {WHNDT(1920, 1080,  2997,  100, 't'), 30, "--tff"            ,  1,  1},
        {WHNDT(1920, 1080,    30,    1, 't'), 30, "--tff"            ,  1,  1},
        {WHNDT(1440, 1080, 30000, 1001, 't'), 30, "--tff"            ,  4,  3},
        {WHNDT(1440, 1080,  2997,  100, 't'), 30, "--tff"            ,  4,  3},
        {WHNDT(1440, 1080,    30,    1, 't'), 30, "--tff"            ,  4,  3},
        /* 720p24 */
        {WHNDT(1280,  720, 24000, 1001, 'p'), 24, ""                 ,  1,  1},
        {WHNDT(1280,  720,    24,    1, 'p'), 24, ""                 ,  1,  1},
        {WHNDT(1280,  720,  2997,  125, 'p'), 24, ""                 ,  1,  1},
        /* 720p25 */
        {WHNDT(1280,  720,    25,    1, 'p'), 25, "--pulldown double",  1,  1},
        /* 720p30 */
        {WHNDT(1280,  720, 30000, 1001, 'p'), 25, "--pulldown double",  1,  1},
        /* 720p50 */
        {WHNDT(1280,  720,    50,    1, 'p'), 50, ""                 ,  1,  1},
        /* 720p60 */
        {WHNDT(1280,  720, 60000, 1001, 'p'), 60, ""                 ,  1,  1},
        {WHNDT(1280,  720,  2997,   50, 'p'), 60, ""                 ,  1,  1},
        /* 576p25 */
        {WHNDT( 720,  576,    25,    1, 'p'), 25, "--fake-interlaced", 16, 11},
        /* 576i25 */
        {WHNDT( 720,  576,    25,    1, 't'), 25, "--tff"            , 16, 11},
        /* 480p24 */
        {WHNDT( 720,  480, 24000, 1001, 'p'), 24, "--pulldown 32"    , 40, 33},
        /* 480i30*/
        {WHNDT( 720,  480, 30000, 1001, 't'), 30, "--tff"            , 40, 33},
        {src_res_fps_type, 0}
    };

#undef WHNDT

    int i = 0;
    while (src_res_fps_type != bd_spec_table[i].res_fps_type)
        i++;

    RETURN_IF_ERROR(!bd_spec_table[i].keyint, -1,
                    "%dx%d @ %d/%d fps not supported.\n",
                    ah->vi->width, ah->vi->height, ah->vi->fps_numerator,
                    ah->vi->fps_denominator);

    char *color = ah->vi->height == 576 ? "bt470bg" :
                  ah->vi->height == 480 ? "smpte170m" :
                  "bt709";

    if (pr->format_type == A2PM_FMT_SDBD) {
        bd_spec_table[i].sar_x = ah->vi->height == 576 ? 12 : 10;
        bd_spec_table[i].sar_y = 11;
    }

    PASS_OR_TRIM;

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
            " --colormatrix %s"
            " --frames %d",
            bd_spec_table[i].keyint, bd_spec_table[i].flag,
            bd_spec_table[i].sar_x, bd_spec_table[i].sar_y,
            color, color, color, ah->vi->num_frames);
    return 0;
}

int act_do_x264raw(params_t *pr, avs_hnd_t *ah, AVS_Value res)
{
    RETURN_IF_ERROR(!avs_has_video(ah->vi), -1, "clip has no video.\n");
    RETURN_IF_ERROR((avs_is_interleaved(ah->vi) || avs_is_yv411(ah->vi)) && pr->yuv_depth > 8, -1,
                    "RGB32/RGB24/YUY2/YV411/Y8 except 8bit-depth is not supported.\n");
    RETURN_IF_ERROR(avs_is_y8(ah->vi) && (ah->vi->width & 1 || ah->vi->height & 1), -1,
                    "in case of Y8, width and height need to be even.\n");
    RETURN_IF_ERROR(pr->yuv_depth > 8 && (ah->vi->width & (avs_is_yv24(ah->vi) ? 1 : 3)), -1,
                    "clip has invalid width %d.\n", ah->vi->width);

    PASS_OR_TRIM;

    char string_for_time[32] = "";
    if (pr->format_type == A2PM_FMT_WITHOUT_TCFILE)
        sprintf(string_for_time, " --fps %d/%d", ah->vi->fps_numerator, ah->vi->fps_denominator);

    fprintf(stdout,
            " - --demuxer %s"
            " --input-csp %s"
            " --input-depth %d"
            " --input-res %dx%d"
            " --output-csp %s"
            " --frames %d"
            "%s",
            get_string_from_pix(ah->vi->pixel_type, TYPE_X264_DEMUXER),
            get_string_from_pix(ah->vi->pixel_type, TYPE_X264_INPUT_CSP),
            pr->yuv_depth,
            pr->yuv_depth > 8 ? ah->vi->width >> 1 : ah->vi->width, ah->vi->height,
            get_string_from_pix(ah->vi->pixel_type, TYPE_X264_OUTPUT_CSP),
            ah->vi->num_frames,
            string_for_time);
    return 0;
}

int act_dump_pix_values_as_txt(params_t *pr, avs_hnd_t *ah, AVS_Value res)
{
    RETURN_IF_ERROR(!avs_has_video(ah->vi), -1, "clip has no video.\n");

    PASS_OR_TRIM;

    a2pm_log(A2PM_LOG_INFO, "writing pixel values of %dx%dx%dframes to stdout as text.\n",
             ah->vi->width, ah->vi->height, ah->vi->num_frames);

    act_do_info(pr, ah);
    fprintf(stdout, "\n\n");

    int wrote = avs_is_planar(ah->vi) ? write_planar_pix_values(ah) :
                                        write_packed_pix_values(ah);
    fflush(stdout);

    a2pm_log(A2PM_LOG_INFO, "finished, wrote %d frames [%d%%].\n",
             wrote, (100 * wrote) / ah->vi->num_frames);
    RETURN_IF_ERROR(wrote != ah->vi->num_frames, -1, "only wrote %d of %d frames.\n",
                    wrote, ah->vi->num_frames);
    return 0;
}
