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

#include "avs2pipemod.h"

int __cdecl
main (int argc, char **argv)
{
    params a2pm_params = {0, 0, 'p', A2PM_NORMAL, NULL, A2P_ACTION_NOTHING};
    parse_opts(argc, argv, &a2pm_params);

    if(a2pm_params.action == A2P_ACTION_NOTHING)
        usage(argv[0]);

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
            do_benchmark(clip, env, input);
            break;
        case A2P_ACTION_NOTHING: // Removing GCC warning, this action is handled above
            break;
    }

    avs_release_clip(clip);
    avs_delete_script_environment(env);

    exit(0);
}

const AVS_VideoInfo *
avisynth_filter(AVS_Clip *clip, AVS_ScriptEnvironment *env,
                const AVS_VideoInfo *info, const char *filter)
{
    AVS_Value val_clip = avs_new_value_clip(clip);
    avs_release_clip(clip);

    AVS_Value val_array[1] = {val_clip};
    AVS_Value val_return = avs_invoke(env, filter, avs_new_value_array(val_array, 1), 0);
    if(avs_is_error(val_return))
        a2p_log(A2P_LOG_ERROR, "%s failed\n", filter);

    clip = avs_take_clip(val_return, env);
    avs_release_value(val_clip);
    avs_release_value(val_return);

    return avs_get_video_info(clip);
}

//inspipred by x264/input/avs.c by Steven Walters.
const AVS_VideoInfo *
avisynth_filter_yuv2yuv(AVS_Clip *clip, AVS_ScriptEnvironment *env,
                        const AVS_VideoInfo *info, const char *filter,
                        a2pm_bool is_interlaced)
{
    AVS_Value val_clip = avs_new_value_clip(clip);
    avs_release_clip(clip);

    const char *arg_name[2] = {NULL, "interlaced"};
    AVS_Value val_array[2] = {val_clip, avs_new_value_bool(is_interlaced)};
    AVS_Value val_return = avs_invoke(env, filter, avs_new_value_array(val_array, 2), arg_name);
    if(avs_is_error(val_return))
        a2p_log(A2P_LOG_ERROR, "%s failed.\n", filter);

    clip = avs_take_clip(val_return, env);
    avs_release_value(val_clip);
    avs_release_value(val_return);

    return avs_get_video_info(clip);
}

const AVS_VideoInfo *
avisynth_filter_rgb2yuv(AVS_Clip *clip, AVS_ScriptEnvironment *env,
                        const AVS_VideoInfo *info, const char *filter,
                        const char *matrix, a2pm_bool is_interlaced)
{
    AVS_Value val_clip = avs_new_value_clip(clip);
    avs_release_clip(clip);

    const char *arg_name[3] = {NULL, "matrix", "interlaced"};
    AVS_Value val_array[3] = {val_clip, avs_new_value_string(matrix),
                              avs_new_value_bool(is_interlaced)};
    AVS_Value val_return = avs_invoke(env, filter, avs_new_value_array(val_array, 3), arg_name);
    if(avs_is_error(val_return))
        a2p_log(A2P_LOG_ERROR, "%s failed.\n", filter);

    clip = avs_take_clip(val_return, env);
    avs_release_value(val_clip);
    avs_release_value(val_return);

    return avs_get_video_info(clip);
}

AVS_Clip *
avisynth_source(char *file, AVS_ScriptEnvironment *env)
{
    AVS_Value val_string = avs_new_value_string(file);
    AVS_Value val_array = avs_new_value_array(&val_string, 1);
    AVS_Value val_return = avs_invoke(env, "Import", val_array, 0);
    avs_release_value(val_array);
    avs_release_value(val_string);

    if(!avs_is_clip(val_return))
        a2p_log(A2P_LOG_ERROR, "avs files return value is not a clip.\n", 0);

    AVS_Clip *clip = avs_take_clip(val_return, env);

    avs_release_value(val_return);

    return clip;
}

int64_t a2pm_gettime(void)
{
    struct timeb tb;
    ftime( &tb );
    return ((int64_t)tb.time * 1000 + (int64_t)tb.millitm) * 1000;
}

char * pix_type_to_string(int pix_type, a2pm_bool is_info)
{
    char *pix_type_string;

    switch(pix_type)
    {
        #ifdef A2P_AVS26
        case AVS_CS_YV24:
            pix_type_string = is_info ? "YV24" : "YUV-444-planar";
            break;
        case AVS_CS_YV16:
            pix_type_string = is_info ? "YV16" : "YUV-422-planar";
            break;
        case AVS_CS_YV411:
            pix_type_string = is_info ? "YV411" : "YUV-411-planar";
            break;
        case AVS_CS_Y8:
            pix_type_string = is_info ? "Y8" : "luma only (gray)";
            break;
        #endif
        case AVS_CS_BGR32:
            pix_type_string = is_info ? "RGB32" : "BGRA";
            break;
        case AVS_CS_BGR24:
            pix_type_string = is_info ? "RGB24" : "BGR";
            break;
        case AVS_CS_YUY2:
            pix_type_string = is_info ? "YUY2" : "YUYV-422-packed";
            break;
        case AVS_CS_YV12:
        case AVS_CS_I420:
            pix_type_string = is_info ? "YV12" : "YUV-420-planar (i420)";
            break;
        default:
            pix_type_string = "UNKNOWN";
    }

    return (char *)pix_type_string;
}

void do_audio(AVS_Clip *clip, AVS_ScriptEnvironment *env, params *params)
{
    const AVS_VideoInfo *info = avs_get_video_info(clip);

    if(!avs_has_audio(info))
        a2p_log(A2P_LOG_ERROR, "clip has no audio.\n", 0);

    if(params->bit) {
        a2p_log(A2P_LOG_INFO, "converting audio to %s.\n", params->bit);
        char tmp[20];
        if(sprintf(tmp, "ConvertAudioTo%s", params->bit) != EOF) {
            const char *filter = tmp;
            info = avisynth_filter(clip, env, info, filter);
        }
    }

    if(_setmode(_fileno(stdout), _O_BINARY) == -1)
        a2p_log(A2P_LOG_ERROR, "cannot switch stdout to binary mode.\n", 0);

    size_t step = 0;
    size_t count = info->audio_samples_per_second;
    size_t size = avs_bytes_per_channel_sample(info) * info->nchannels;
    uint64_t wrote = 0;
    uint64_t target = info->num_audio_samples;

    void *buff = malloc(count * size);

    a2p_log(A2P_LOG_INFO, "writing %.3f seconds of %d Hz, %d channel audio.\n",
            (double)info->num_audio_samples / info->audio_samples_per_second,
            info->audio_samples_per_second, info->nchannels);

    setvbuf(stdout, NULL, _IOFBF, BUFSIZE_OF_STDOUT);

    int64_t start = a2pm_gettime();

    if(params->fmt_type != A2PM_RAW) {
        WaveFormatType format = (info->sample_type == AVS_SAMPLE_FLOAT) ?
                                WAVE_FORMAT_IEEE_FLOAT :
                                WAVE_FORMAT_PCM;
        if (params->fmt_type == A2PM_EXTRA) {
            WaveRiffExtHeader *header
                = wave_create_riff_ext_header(format, info->nchannels,
                                              info->audio_samples_per_second,
                                              avs_bytes_per_channel_sample(info),
                                              info->num_audio_samples);
            fwrite(header, sizeof(*header), 1, stdout);
            free(header);
        } else {
            WaveRiffHeader *header
                = wave_create_riff_header(format, info->nchannels,
                                          info->audio_samples_per_second,
                                          avs_bytes_per_channel_sample(info),
                                          info->num_audio_samples);
            fwrite(header, sizeof(*header), 1, stdout);
            free(header);
        }
    }

    uint64_t i;
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

    int64_t end = a2pm_gettime();
    int64_t elapsed = end - start;
    a2p_log(A2P_LOG_INFO, "total elapsed time is %.3f sec.\n", elapsed / 1000000.0);

    if(wrote != target)
        a2p_log(A2P_LOG_ERROR, "only wrote %I64u of %I64u samples.\n", wrote, target);
}

void do_y4m(AVS_Clip *clip, AVS_ScriptEnvironment *env, params *params)
{
    const AVS_VideoInfo *info = avs_get_video_info(clip);

    if(!avs_has_video(info))
        a2p_log(A2P_LOG_ERROR, "clip has no video.\n");

    if(avs_is_field_based(info)) {
        a2p_log(A2P_LOG_WARNING, "clip is FieldBased.\n");
        fprintf(stderr, "           yuv4mpeg2's spec doesn't support fieldbased clip.\n"
                        "           choose what you want.\n"
                        "           1:add AssumeFrameBased()  2:add Weave()  others:exit\n"
                        "               input a number and press enter : ");

        int k;
        fscanf(stdin, "%d", &k);
        switch(k) {
            case 1:
                info = avisynth_filter(clip, env, info, "AssumeFrameBased");
                a2p_log(A2P_LOG_INFO, "clip was assumed that FrameBased.\n", 0);
                break;
            case 2:
                info = avisynth_filter(clip, env, info, "Weave");
                a2p_log(A2P_LOG_INFO, "clip was Weaved\n", 0);
                break;
            default:
                a2p_log(A2P_LOG_ERROR, "good bye...\n", 2);
        }
    }

    yuvout y4mout = {3, 1, 1, "420mpeg2"};
    const char *matrix = info->height < 720 ? A2PM_BT601 : A2PM_BT709;
    #ifdef A2P_AVS26
    a2pm_csp_handle26(info->pixel_type, &y4mout);

    if(avs_is_rgb(info)) {
        a2p_log(A2P_LOG_INFO, "converting video to planar yuv444 with %s coefficients.\n", matrix);
        info = avisynth_filter_rgb2yuv(clip, env, info, "ConvertToYV24", matrix, 0);
    } else if(!avs_is_planar(info)) {
        a2p_log(A2P_LOG_INFO, "converting video to planar yuv422.\n", 0);
        info = avisynth_filter_yuv2yuv(clip, env, info, "ConvertToYV16", 0);
    }
    #else
    if((info->pixel_type != AVS_CS_I420) && (info->pixel_type != AVS_CS_YV12)) {
        if(info->width & 1 || info->height & 1 || (params->ip != 'p' && info->height % 4))
            a2p_log(A2P_LOG_ERROR, "invalid resolution, converting failed.\n");
        else if(avs_is_rgb(info)) {
            a2p_log(A2P_LOG_INFO, "converting video to planar yuv420 with %s coefficients.\n", matrix);
            info=avisynth_filter_rgb2yuv(clip, env, info, "ConvertToYV12",
                                         info->height < 720 ? A2PM_BT601 : A2PM_BT709,
                                         params->ip == 'p' ? 0 : 1);
        } else {
            a2p_log(A2P_LOG_INFO, "converting video to planar yuv420.\n", 0);
            info = avisynth_filter_yuv2yuv(clip, env, info, "ConvertToYV12",
                                           params->ip == 'p' ? 0 : 1);
        }
    }
    #endif
    if(_setmode(_fileno(stdout), _O_BINARY) == -1)
        a2p_log(A2P_LOG_ERROR, "cannot switch stdout to binary mode.\n");

    a2p_log(A2P_LOG_INFO, "writing %d frames of %d/%d fps, %dx%d, sar %d:%d, YUV%s %s video.\n",
            info->num_frames, info->fps_numerator, info->fps_denominator,
            info->width, info->height, params->sar_x, params->sar_y, y4mout.y4m_ctag_value,
            params->ip == 'p' ? "progressive" : params->ip == 't' ? "tff" : "bff");

    setvbuf(stdout, NULL, _IOFBF, BUFSIZE_OF_STDOUT);

    int64_t start = a2pm_gettime();

    // YUV4MPEG2 stream header http://linux.die.net/man/5/yuv4mpeg
    fprintf(stdout, "YUV4MPEG2 W%d H%d F%u:%u I%c A%d:%d C%s\n", info->width,
            info->height, info->fps_numerator, info->fps_denominator,
            params->ip, params->sar_x, params->sar_y, y4mout.y4m_ctag_value);

    int wrote = a2pm_write_planar_frames(clip, info, &y4mout, A2PM_FALSE);

    fflush(stdout); // clear buffers before we exit

    int64_t end = a2pm_gettime();
    int64_t elapsed = end - start;

    a2p_log(A2P_LOG_REPEAT, "finished, wrote %d frames [%d%%].\n",
        wrote, (100 * wrote) / info->num_frames);

    a2p_log(A2P_LOG_INFO, "total elapsed time is %.3f sec [%.3ffps].\n",
        elapsed / 1000000.0, wrote * 1000000.0 / elapsed);

    if(wrote != info->num_frames)
        a2p_log(A2P_LOG_ERROR, "only wrote %d of %d frames.\n", wrote, info->num_frames);
}

void do_rawvideo(AVS_Clip *clip, AVS_ScriptEnvironment *env, params *params)
{
    const AVS_VideoInfo *info = avs_get_video_info(clip);
    yuvout rawout = {3, 1, 1, ""};

    if(!avs_has_video(info))
        a2p_log(A2P_LOG_ERROR, "clip has no video.\n");

    if(params->fmt_type == A2PM_EXTRA) {
        info = avisynth_filter(clip, env, info, "FlipVertical");
        a2p_log(A2P_LOG_INFO, "Video has flipped.\n");
    }

    a2pm_bool is_planar;
    switch(info->pixel_type) {
        case AVS_CS_BGR32:
        case AVS_CS_BGR24:
        case AVS_CS_YUY2:
        #ifdef A2P_AVS26
        case AVS_CS_Y8:
        #endif
            is_planar = A2PM_FALSE;
            break;
        default:
            is_planar = A2PM_TRUE;
            #ifdef A2P_AVS26
            a2pm_csp_handle26(info->pixel_type, &rawout);
            #endif
    }

    char *pix = pix_type_to_string(info->pixel_type, A2PM_FALSE);

    if(_setmode(_fileno(stdout), _O_BINARY) == -1)
        a2p_log(A2P_LOG_ERROR, "cannot switch stdout to binary mode.\n");

    a2p_log(A2P_LOG_INFO, "writing %d frames of %dx%d %s rawvideo.\n",
            info->num_frames, info->width, info->height, pix);

    setvbuf(stdout, NULL, _IOFBF, BUFSIZE_OF_STDOUT);

    int64_t start = a2pm_gettime();

    int wrote = is_planar ?
                a2pm_write_planar_frames(clip, info, &rawout, A2PM_TRUE) :
                a2pm_write_packed_frames(clip, info);

    fflush(stdout);

    int64_t end = a2pm_gettime();
    int64_t elapsed = end - start;

    a2p_log(A2P_LOG_REPEAT, "finished, wrote %d frames [%d%%].\n",
        wrote, (100 * wrote) / info->num_frames);

    a2p_log(A2P_LOG_INFO, "total elapsed time is %.3f sec [%.3ffps].\n",
        elapsed / 1000000.0, wrote * 1000000.0 / elapsed);

    if(wrote != info->num_frames)
        a2p_log(A2P_LOG_ERROR, "only wrote %d of %d frames.\n", wrote, info->num_frames);
}

void do_info(AVS_Clip *clip, AVS_ScriptEnvironment *env, char *input, a2pm_bool need_audio)
{
    const AVS_VideoInfo *info = avs_get_video_info(clip);

    if(avs_has_video(info)) {
        fprintf(stdout,
                "script_name      %s\n"
                "v:width          %d\n"
                "v:height         %d\n"
                "v:fps            %d/%d\n"
                "v:frames         %d\n"
                "v:duration[sec]  %.3f\n"
                "v:image_type     %s\n",
                input, info->width, info->height, info->fps_numerator,
                info->fps_denominator, info->num_frames,
                (double)info->num_frames * info->fps_denominator / info->fps_numerator,
                !avs_is_field_based(info) ? "framebased" : "fieldbased");

        char *field_order;
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
        fprintf(stdout,
                "v:field_order    %s\n"
                "v:pixel_type     %s\n",
                field_order, pix_type_to_string(info->pixel_type, A2PM_TRUE));
    }

    if(avs_has_audio(info) && need_audio) {
        fprintf(stdout,
                "a:sample_rate    %d\n"
                "a:format         %s\n"
                "a:bit_depth      %d\n"
                "a:channels       %d\n"
                "a:samples        %I64d\n"
                "a:duration[sec]  %.3f\n",
                info->audio_samples_per_second, 
                info->sample_type == AVS_SAMPLE_FLOAT ? "float" : "pcm",
                avs_bytes_per_channel_sample(info) * 8, info->nchannels,
                info->num_audio_samples,
                (double)info->num_audio_samples / info->audio_samples_per_second);
    }
}

void do_benchmark(AVS_Clip *clip, AVS_ScriptEnvironment *env, char *input)
{
    const AVS_VideoInfo *info = avs_get_video_info(clip);

    if(!avs_has_video(info))
        a2p_log(A2P_LOG_ERROR, "clip has no video.\n");

    do_info(clip, env, input, A2PM_FALSE);

    AVS_VideoFrame *frame;
    int count = info->num_frames / BM_FRAMES_PAR_OUT;
    int passed = 0;
    int percentage;
    int64_t elapsed, running;
    double fps;

    a2p_log(A2P_LOG_INFO, "benchmarking %d frames video.\n", info->num_frames);

    int64_t start = a2pm_gettime();

    for(int i = 0; i < count; i++) {
        for(int f = 0; f < BM_FRAMES_PAR_OUT; f++) {
            frame = avs_get_frame(clip, passed);
            avs_release_frame(frame);
            passed++;
        }

        running = a2pm_gettime();
        elapsed = running - start;
        fps = passed * 1000000.0 / elapsed;
        percentage = passed * 100 / info->num_frames;

        a2p_log(A2P_LOG_REPEAT, "[elapsed %.3f sec] %d/%d frames [%3d%%][%.3ffps]",
                elapsed / 1000000.0, passed, info->num_frames, percentage, fps);
    }

    while(passed < info->num_frames) {
        frame = avs_get_frame(clip, passed);
        avs_release_frame(frame);
        passed++;
    }

    running = a2pm_gettime();
    elapsed = running - start;
    fps = passed * 1000000.0 / elapsed;

    fprintf(stderr, "\n");
    a2p_log(A2P_LOG_INFO, "%d frames has passed [100%%][%.3ffps]\n", passed, fps);
    fprintf(stdout, "benchmark result: total elapsed time is %.3f sec [%.3ffps]\n",
            elapsed / 1000000.0, fps);

    if(passed != info->num_frames)
        a2p_log(A2P_LOG_ERROR, "only passed %d of %d frames.\n", passed, info->num_frames);
}

void do_x264bd(AVS_Clip *clip, AVS_ScriptEnvironment *env, params *params)
{
    // x264 arguments from http://sites.google.com/site/x264bluray/
    // resolutions, fps... http://forum.doom9.org/showthread.php?t=154533

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

    const AVS_VideoInfo *info = avs_get_video_info(clip);

    if(!((info->height == 480) || (info->height == 576)) && (params->sar_x == 10))
        a2p_log(A2P_LOG_ERROR, "DAR 4:3 is supported only ntsc/pal sd source\n");

    int keyint;
    char * args;
    char * color;

    // Initial format guess, setting ref and color.
    // max safe width or height = 32767
    // width << 16         | height
    // 0000 0000 0000 0000   0000 0000  0000 0000
    switch((info->width << 16) | info->height) {
        case (1920 << 16) | 1080: // HD1080
        case (1440 << 16) | 1080:
            res = A2P_RES_1080;
            color = "bt709";
            if(info->width == 1440)
            {
                params->sar_x = 4;
                params->sar_y = 3;
            }
            break;
        case (1280 << 16) | 720: // HD720
            res = A2P_RES_720;
            color = "bt709";
            break;
        case (720 << 16) | 576: // PAL
            res = A2P_RES_576;
            color = "bt470bg";
            if(params->sar_x == 10) {
                params->sar_x = 12;
                params->sar_y = 11;
            } else {
                params->sar_x = 16;
                params->sar_y = 11;
            }
            break;
        case (720 << 16) | 480: // NTSC
            res = A2P_RES_480;
            color = "smpte170m";
            if(params->sar_x == 1) {
                params->sar_x = 40;
                params->sar_y = 33;
            }
            break;
        default:
            res = A2P_RES_UNKNOWN;
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
        default:
            fps = A2P_FPS_UNKNOWN;
            keyint = 0;
            break;
    }

    // Ensure video is supported... this is messy
    // and set any special case arguments.
    // res << 16           | fps << 8  | !!(params.ip != 'p')
    // 0000 0000 0000 0000   0000 0000   0000 0000
    switch((res << 16) | fps << 8 | !!(params->ip != 'p')) {
        case ((A2P_RES_1080 << 16) | (A2P_FPS_25 << 8) | 1):
        case ((A2P_RES_1080 << 16) | (A2P_FPS_29 << 8) | 1):
        case ((A2P_RES_1080 << 16) | (A2P_FPS_30 << 8) | 1):
        case ((A2P_RES_576 << 16) | (A2P_FPS_25 << 8) | 1):
        case ((A2P_RES_480 << 16) | (A2P_FPS_29 << 8) | 1):
            args = "--tff";
            break;
        case ((A2P_RES_1080 << 16) | (A2P_FPS_29 << 8) | 0):
        case ((A2P_RES_1080 << 16) | (A2P_FPS_25 << 8) | 0):
        case ((A2P_RES_576 << 16) | (A2P_FPS_25 << 8) | 0):
            args = "--fake-interlaced";
            break;
        case ((A2P_RES_720 << 16) | (A2P_FPS_29 << 8) | 0):
        case ((A2P_RES_720 << 16) | (A2P_FPS_25 << 8) | 0):
            args = "--pulldown double";
            break;
        //case ((A2P_RES_480 << 16) | (A2P_FPS_29 << 8) | 0):
        //    args = "--pulldown 32 --fake-interlaced";
        //    break;
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
            "--bluray-compat --vbv-maxrate 40000 --vbv-bufsize 30000"
            " --level 4.1 --keyint %d --open-gop --slices 4 %s --sar %d:%d"
            " --colorprim %s --transfer %s --colormatrix %s",
            keyint, args, params->sar_x, params->sar_y, color, color, color);
}

void parse_opts(int argc, char **argv, params *params)
{
    char short_opts[] = "a::Bb::e::ip::t::v::w::x::";
    struct option long_opts[] = {
        {"wav",      optional_argument, NULL, 'w'},
        {"extwav",   optional_argument, NULL, 'e'},
        {"rawaudio", optional_argument, NULL, 'a'},
        {"y4mp",     optional_argument, NULL, 'p'},
        {"y4mt",     optional_argument, NULL, 't'},
        {"y4mb",     optional_argument, NULL, 'b'},
        {"rawvideo", optional_argument, NULL, 'v'},
        {"info",           no_argument, NULL, 'i'},
        {"x264bdp",  optional_argument, NULL, 'x'},
        {"x264bdt",  optional_argument, NULL, 'y'},
        {"benchmark",      no_argument, NULL, 'B'},
        {0,0,0,0}
    };
    int parse = 0;
    int index = 0;

    while((parse = getopt_long_only(argc, argv, short_opts, long_opts, &index)) != -1) {
        switch(parse) {
            case 'a':
            case 'e':
            case 'w':
                params->action = A2P_ACTION_AUDIO;
                if(optarg)
                    params->bit = optarg;
                params->fmt_type = (parse == 'w') ? A2PM_NORMAL :
                                   (parse == 'a') ? A2PM_RAW :
                                                    A2PM_EXTRA;
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
            default:
                params->action = A2P_ACTION_NOTHING;
        }
    }
}

#ifdef A2P_AVS26
void a2pm_csp_handle26(int pix_type, yuvout *data)
{
    switch(pix_type) {
        case AVS_CS_BGR32:
        case AVS_CS_BGR24:
        case AVS_CS_YV24:
            data->y4m_ctag_value = "444";
            data->h_uv = 0;
            data->v_uv = 0;
            break;
        case AVS_CS_YUY2:
        case AVS_CS_YV16:
            data->y4m_ctag_value = "422";
            data->h_uv = 1;
            data->v_uv = 0;
            break;
        case AVS_CS_YV411:
            data->y4m_ctag_value = "411";
            data->h_uv = 2;
            data->v_uv = 0;
            break;
        case AVS_CS_Y8:
            data->num_planes = 1;
            data->y4m_ctag_value = "mono";
            data->h_uv = 0;
            data->v_uv = 0;
            break;
        default:
            break;
    }
}
#endif

int a2pm_write_planar_frames(AVS_Clip *clip, const AVS_VideoInfo *info, yuvout *yuvout, a2pm_bool is_raw)
{
    AVS_VideoFrame *frame;
    const BYTE *buff ;
    size_t count = (info->width * info->height * avs_bits_per_pixel(info)) >> 3;
    int wrote = 0;
    const int planes[] = {AVS_PLANAR_Y, AVS_PLANAR_U, AVS_PLANAR_V};
    const char *header = is_raw ? "" : Y4M_FRAME_HEADER_STRING;

    for(int f = 0; f < info->num_frames; f++) {
        fprintf(stdout, header);
        size_t step = 0;
        frame = avs_get_frame(clip, f);

        for(int p = 0; p < yuvout->num_planes; p++) {
            int w = info->width >> (p ? yuvout->h_uv : 0);
            int h = info->height >> (p ? yuvout->v_uv : 0);
            int pitch = avs_get_pitch_p(frame, planes[p]);
            buff = avs_get_read_ptr_p(frame, planes[p]);

            for(int i = 0; i < h; i++) {
                step += fwrite(buff, sizeof(BYTE), w, stdout);
                buff += pitch;
            }
        }
        avs_release_frame(frame);

        if(step != count)
            return wrote;

        wrote++;
    }
    return wrote;
}

int a2pm_write_packed_frames(AVS_Clip *clip, const AVS_VideoInfo *info)
{
    AVS_VideoFrame *frame;
    int w = info->width * avs_bits_per_pixel(info) >> 3;
    int h = info->height;
    int wrote = 0;
    size_t count = w * h;
    const BYTE *buff;

    for(int f = 0; f < info->num_frames; f++) {
        size_t step = 0;
        frame = avs_get_frame(clip, f);
        int pitch = avs_get_pitch(frame);
        buff = avs_get_read_ptr(frame);

        for(int i = 0; i < h; i++) {
            step += fwrite(buff, sizeof(BYTE), w, stdout);
            buff += pitch;
        }
    avs_release_frame(frame);

    if(step != count)
        return wrote;

    wrote++;
    }
    return wrote;
}

void usage(char *binary)
{
    #ifdef A2P_AVS26
        fprintf(stderr, "avs2pipemod for AviSynth 2.6.0 Alpha 2 or later");
    #else
        fprintf(stderr, "avs2pipemod for AviSynth 2.5.8");
    #endif

    fprintf(stderr,
            "  %s\n"
            "build on %s\n"
            "\n"
            "Usage: %s [one option] input.avs\n"
            "  e.g. avs2pipemod -audio=24bit input.avs > output.wav\n"
            "       avs2pipemod -y4mt=10:11 input.avs | x264 - --demuxer y4m -o tff.mkv\n"
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
            "   -x264bdt[=4:3  default uset(16:9)]\n"
            "       suggest x264(r1939 or later) arguments for bluray disc encoding\n"
            "      in case of tff source.\n"
            "       set optarg if DAR4:3 is required(ntsc/pal sd source).\n"
            "\n"
            "   -info  - output information about aviscript clip.\n"
            "\n"
            "   -benchmark - do benchmark aviscript, and output results to stdout.\n"
            "\n"
            "note  : in yuv4mpeg2 output mode, RGB input that has 720pix height or more\n"
            "       will be converted to YUV with Rec.709 coefficients instead of Rec.601.\n"
            "\n"
            "note2 : '-x264bdp/t' supports only for primary stream encoding.\n"
            "\n"
            "note3 : '-extaudio' supports only general speaker positions.\n"
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