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

#include "avs2pipemod.h"

int __cdecl
main (int argc, char **argv)
{
    struct params a2pm_params = {0, 0, 'p', 0, "", A2P_ACTION_NOTHING};
    a2pm_params = parse_opts(argc, argv, a2pm_params);

    if(a2pm_params.action == A2P_ACTION_NOTHING)
        usage(argv[0]);

    char *input = argv[argc - 1];
    AVS_ScriptEnvironment *env = avs_create_script_environment(AVISYNTH_INTERFACE_VERSION);
    AVS_Clip *clip = avisynth_source(input, env);

    switch(a2pm_params.action)
    {
        case A2P_ACTION_AUDIO:
            do_audio(clip, env, a2pm_params);
            break;

        case A2P_ACTION_Y4M:
            do_y4m(clip, env, a2pm_params);
            break;

        case A2P_ACTION_PACKEDRAW:
            do_packedraw(clip, env);
            break;

        case A2P_ACTION_INFO:
            do_info(clip, env, input, NEED_AUDIO_INFO);
            break;

        case A2P_ACTION_X264BD:
            do_x264bd(clip, env, a2pm_params);
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

AVS_Clip *
avisynth_filter(AVS_Clip *clip, AVS_ScriptEnvironment *env, const char *filter)
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

    return clip;
}

//inspipred by x264/input/avs.c by Steven Walters.
AVS_Clip *
avisynth_filter_yuv2yuv(AVS_Clip *clip, AVS_ScriptEnvironment *env, const char *filter, int interlaced)
{
    AVS_Value val_clip = avs_new_value_clip(clip);
    avs_release_clip(clip);

    const char *arg_name[2] = {NULL, "interlaced"};
    AVS_Value val_array[2] = {val_clip, avs_new_value_bool(interlaced)};
    AVS_Value val_return = avs_invoke(env, filter, avs_new_value_array(val_array, 2), arg_name);
    if(avs_is_error(val_return))
        a2p_log(A2P_LOG_ERROR, "%s failed.\n", filter);

    clip = avs_take_clip(val_return, env);

    avs_release_value(val_clip);
    avs_release_value(val_return);

    return clip;
}

AVS_Clip *
avisynth_filter_rgb2yuv(AVS_Clip *clip, AVS_ScriptEnvironment *env, const char *filter, const char *matrix, int interlaced)
{
    AVS_Value val_clip = avs_new_value_clip(clip);
    avs_release_clip(clip);

    const char *arg_name[3] = {NULL, "matrix", "interlaced"};
    AVS_Value val_array[3] = {val_clip, avs_new_value_string(matrix), avs_new_value_bool(interlaced)};
    AVS_Value val_return = avs_invoke(env, filter, avs_new_value_array(val_array, 3), arg_name);
    if(avs_is_error(val_return))
        a2p_log(A2P_LOG_ERROR, "%s failed.\n", filter);

    clip = avs_take_clip(val_return, env);

    avs_release_value(val_clip);
    avs_release_value(val_return);

    return clip;
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

char * pix_type_to_string(int pix_type)
{
    char *pix_type_string;

    switch(pix_type)
    {
        #ifdef A2P_AVS26
        case AVS_CS_YV24:
            pix_type_string = "YV24 (YVU-444-planar)";
            break;

        case AVS_CS_YV16:
            pix_type_string = "YV16 (YVU-422-planar)";
            break;

        case AVS_CS_YV411:
            pix_type_string = "YV411 (YVU-411-planar)";
            break;

        case AVS_CS_Y8:
            pix_type_string = "Y8 (luma only)";
            break;
        #endif

        case AVS_CS_BGR32:
            pix_type_string = "RGB32 (BGRA)";
            break;

        case AVS_CS_BGR24:
            pix_type_string = "RGB24 (BGR)";
            break;

        case AVS_CS_YUY2:
            pix_type_string = "YUY2 (YUYV-422-packed)";
            break;

        case AVS_CS_YV12:
            pix_type_string = "YV12 (YVU-420-planar)";
            break;

        case AVS_CS_I420:
            pix_type_string = "I420 (YUV-420-planar)";
            break;

        default:
            pix_type_string = "UNKNOWN";
    }

    return (char *)pix_type_string;
}

void
do_audio(AVS_Clip *clip, AVS_ScriptEnvironment *env, struct params params)
{
    const AVS_VideoInfo *info = avs_get_video_info(clip);

    if(!avs_has_audio(info))
        a2p_log(A2P_LOG_ERROR, "clip has no audio.\n", 0);

    if(params.bit)
    {
        a2p_log(A2P_LOG_INFO, "converting audio to %s.\n", params.bit);
        char tmp[20];

        if(sprintf(tmp, "ConvertAudioTo%s", params.bit) != EOF)
        {
            const char *filter = tmp;
            clip = avisynth_filter(clip, env, filter);
        }

        info = avs_get_video_info(clip);
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

    if(!params.is_raw)
    {
        WaveFormatType format = (info->sample_type == AVS_SAMPLE_FLOAT) ?
                                WAVE_FORMAT_IEEE_FLOAT :
                                WAVE_FORMAT_PCM;
        WaveRiffHeader *header = wave_create_riff_header(format, info->nchannels,
                                                         info->audio_samples_per_second,
                                                         avs_bytes_per_channel_sample(info),
                                                         info->num_audio_samples);
        fwrite(header, sizeof(*header), 1, stdout);
        free(header);
    }

    uint64_t i;
    for (i = 0; i < target; i += count)
    {
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

void
do_y4m(AVS_Clip *clip, AVS_ScriptEnvironment *env, struct params params)
{
    const AVS_VideoInfo *info = avs_get_video_info(clip);

    if(!avs_has_video(info))
        a2p_log(A2P_LOG_ERROR, "clip has no video.\n");

    if(avs_is_field_based(info))
    {
        a2p_log(A2P_LOG_WARNING, "clip is FieldBased.\n");
        fprintf(stderr, "           yuv4mpeg2's spec doesn't support FieldBased clip.\n"
                        "           choose what you want.\n"
                        "           1:add AssumeFrameBased()  2:add Weave()  others:exit\n"
                        "               input a number and press enter : ");

        int k;
        fscanf(stdin, "%d", &k);

        switch(k)
        {
            case 1:
                clip = avisynth_filter(clip, env, "AssumeFrameBased");
                info = avs_get_video_info(clip);
                a2p_log(A2P_LOG_INFO, "clip was assumed that FrameBased.\n", 0);
                break;

            case 2:
                clip = avisynth_filter(clip, env, "Weave");
                info = avs_get_video_info(clip);
                a2p_log(A2P_LOG_INFO, "clip was Weaved\n", 0);
                break;

            default:
                a2p_log(A2P_LOG_ERROR, "good bye...\n", 2);
        }
    }

    // Number of planes normally 3;
    int np = 3;
    int32_t h_uv, v_uv;
    char *yuv_csp;
    const char *matrix = (info->height < 720) ? A2PM_BT601 : A2PM_BT709;

    #ifdef A2P_AVS26
    switch(info->pixel_type)
    {
        case AVS_CS_BGR32:
        case AVS_CS_BGR24:
            a2p_log(A2P_LOG_INFO, "converting video to planar yuv444.\n", 0);
            clip = avisynth_filter_rgb2yuv(clip, env, "ConvertToYV24", matrix, 0);
            info = avs_get_video_info(clip);
        case AVS_CS_YV24:
            yuv_csp = "444";
            h_uv = 0;
            v_uv = 0;
            break;

        case AVS_CS_YUY2:
            a2p_log(A2P_LOG_INFO, "converting video to planar yuv422.\n", 0);
            clip = avisynth_filter_yuv2yuv(clip, env, "ConvertToYV16", 0);
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
            if((info->pixel_type != AVS_CS_I420) && (info->pixel_type != AVS_CS_YV12))
            {
                a2p_log(A2P_LOG_INFO, "converting video to planar yuv420.\n", 0);

                if(info->width & 1 || info->height & 1 || (params.ip != 'p' && info->height % 4))
                    a2p_log(A2P_LOG_ERROR, "invalid resolution, converting failed.\n");

                clip = avs_is_rgb(info) ?
                    avisynth_filter_rgb2yuv(clip, env, "ConvertToYV12", matrix,
                                            params.ip == 'p' ? 0 : 1) :
                    avisynth_filter_yuv2yuv(clip, env, "ConvertToYV12",
                                            params.ip == 'p' ? 0 : 1);

                info = avs_get_video_info(clip);
            }

            yuv_csp = "420mpeg2"; //same as avisynth default subsampling type of yuv420
            h_uv = 1;
            v_uv = 1;

    #ifdef A2P_AVS26
    }
    #endif

    if(!avs_is_planar(info))
        a2p_log(A2P_LOG_ERROR, "colorspace handling failed. leaving...\n", 2);

    size_t count = (info->width * info->height * avs_bits_per_pixel(info)) >> 3;

    if(_setmode(_fileno(stdout), _O_BINARY) == -1)
        a2p_log(A2P_LOG_ERROR, "cannot switch stdout to binary mode.\n");

    a2p_log(A2P_LOG_INFO, "writing %d frames of %d/%d fps, %dx%d, sar %d:%d YUV%s %s video.\n",
            info->num_frames, info->fps_numerator, info->fps_denominator,
            info->width, info->height, params.sar_x, params.sar_y, yuv_csp,
            params.ip == 'p' ? "progressive" : params.ip == 't' ? "tff" : "bff");

    setvbuf(stdout, NULL, _IOFBF, BUFSIZE_OF_STDOUT);

    int64_t start = a2pm_gettime();

    // YUV4MPEG2 header http://linux.die.net/man/5/yuv4mpeg
    fprintf(stdout, "YUV4MPEG2 W%d H%d F%u:%u I%c A%d:%d C%s\n", info->width,
            info->height, info->fps_numerator, info->fps_denominator,
            params.ip, params.sar_x, params.sar_y, yuv_csp);

    // method from avs2yuv converted to c

    AVS_VideoFrame *frame;
    static const int planes[] = {AVS_PLANAR_Y, AVS_PLANAR_U, AVS_PLANAR_V};
    const BYTE *buff;
    size_t step;
    int32_t target = info->num_frames;
    int32_t wrote = 0, f, i, p, w, h, pitch;

    for(f = 0; f < target; f++)
    {
        fprintf(stdout, "FRAME\n");
        step = 0;
        frame = avs_get_frame(clip, f);

        for(p = 0; p < np; p++)
        {
            w = info->width >> (p ? h_uv : 0);
            h = info->height >> (p ? v_uv : 0);
            pitch = avs_get_pitch_p(frame, planes[p]);
            buff = avs_get_read_ptr_p(frame, planes[p]);

            for(i = 0; i < h; i++)
            {
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

    int64_t end = a2pm_gettime();
    int64_t elapsed = end - start;

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
    const AVS_VideoInfo *info = avs_get_video_info(clip);

    if(!avs_has_video(info))
        a2p_log(A2P_LOG_ERROR, "clip has no video.\n");

    char *pix;
    switch(info->pixel_type)
    {
        case AVS_CS_BGR32:
        case AVS_CS_BGR24:
        case AVS_CS_YUY2:
        #ifdef A2P_AVS26
        case AVS_CS_Y8:
        #endif
            pix = pix_type_to_string(info->pixel_type);
            break;

        default:
            pix = "";
            a2p_log(A2P_LOG_ERROR, "invalid pixel_type in this mode.\n", 0);
    }

    int32_t w = info->width * avs_bits_per_pixel(info) >> 3;
    int32_t h = info->height;
    size_t count = w * h;

    if(_setmode(_fileno(stdout), _O_BINARY) == -1)
        a2p_log(A2P_LOG_ERROR, "cannot switch stdout to binary mode.\n");

    a2p_log(A2P_LOG_INFO, "writing %d frames of %dx%d %s rawvideo.\n",
            info->num_frames, info->width, info->height, pix);

    int32_t target = info->num_frames;
    int32_t wrote = 0;
    const BYTE *buff;
    AVS_VideoFrame *frame;
    setvbuf(stdout, NULL, _IOFBF, BUFSIZE_OF_STDOUT);

    int64_t start = a2pm_gettime();

    int32_t f, i, pitch;
    size_t step;

    for(f = 0; f < target; f++)
    {
        step = 0;
        frame = avs_get_frame(clip, f);
        pitch = avs_get_pitch(frame);
        buff = avs_get_read_ptr(frame);

        for(i = 0; i < h; i++)
        {
            step += fwrite(buff, sizeof(BYTE), w, stdout);
            buff += pitch;
        }

        avs_release_frame(frame);

        if(step != count)
            break;

        wrote++;
    }

    int64_t end = a2pm_gettime();
    int64_t elapsed = end - start;

    a2p_log(A2P_LOG_REPEAT, "finished, wrote %d frames [%d%%].\n",
        wrote, (100 * wrote) / target);

    a2p_log(A2P_LOG_INFO, "total elapsed time is %.3f sec [%.3ffps].\n",
        elapsed / 1000000.0, wrote * 1000000.0 / elapsed);

    if(wrote != target)
        a2p_log(A2P_LOG_ERROR, "only wrote %d of %d frames.\n", wrote, target);
}

void
do_info(AVS_Clip *clip, AVS_ScriptEnvironment *env, char *input, int audio)
{
    const AVS_VideoInfo *info = avs_get_video_info(clip);

    if(avs_has_video(info))
    {
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

        char *field_order;
        switch(info->image_type)
        {
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
        fprintf(stdout, "v:pixel_type     %s\n", pix_type_to_string(info->pixel_type));
    }

    if(avs_has_audio(info) && audio)
    {
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
    const AVS_VideoInfo *info = avs_get_video_info(clip);

    if(!avs_has_video(info))
        a2p_log(A2P_LOG_ERROR, "clip has no video.\n");

    do_info(clip, env, input, NOT_NEED_AUDIO_INFO);

    AVS_VideoFrame *frame;
    int32_t target = info->num_frames;
    int32_t count = target / BM_FRAMES_PAR_OUT;
    int32_t passed = 0, percentage;
    int64_t elapsed, running;
    double fps;

    a2p_log(A2P_LOG_INFO, "benchmarking %d frames video.\n", target);

    int64_t start = a2pm_gettime();

    int32_t f, i;
    for(i = 0; i < count; i++)
    {
        for(f = 0; f < BM_FRAMES_PAR_OUT; f++)
        {
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

    while(passed < target)
    {
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

    if(passed != target)
        a2p_log(A2P_LOG_ERROR, "only passed %d of %d frames.\n", passed, target);
}

void
do_x264bd(AVS_Clip *clip, AVS_ScriptEnvironment *env, struct params params)
{
    // x264 arguments from http://sites.google.com/site/x264bluray/
    // resolutions, fps... http://forum.doom9.org/showthread.php?t=154533
    //   vbv-maxrate reduced to 15000 from 40000 for DVD playback
    //   ref added to all settings for simplicity

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
    char * args;
    char * color;
    char * sar;

    const AVS_VideoInfo *info = avs_get_video_info(clip);

    // Initial format guess, setting ref and color.
    // max safe width or height = 32767
    // width << 16         | height
    // 0000 0000 0000 0000   0000 0000  0000 0000
    switch((info->width << 16) | info->height)
    {
        case (1920 << 16) | 1080: // HD1080
        case (1440 << 16) | 1080:
            res = A2P_RES_1080;
            color = "bt709";
            sar = info->width == 1920 ? "1:1" : "4:3";
            break;

        case (1280 << 16) | 720: // HD720
            res = A2P_RES_720;
            color = "bt709";
            sar = "1:1";
            break;

        case (720 << 16) | 576: // PAL
            res = A2P_RES_576;
            color = "bt470bg";
            sar = "16:11";
            break;

        case (720 << 16) | 480: // NTSC
            res = A2P_RES_480;
            color = "smpte170m";
            sar = "40:33";
            break;

        default:
            res = A2P_RES_UNKNOWN;
            color = "";
            sar = "";
            break;
    }

    // See http://avisynth.org/mediawiki/FPS
    // fps_numerator << 16 | fps_denominator
    // 0000 0000 0000 0000   0000 0000  0000 0000
    switch((info->fps_numerator << 16) | info->fps_denominator)
    {
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
    switch((res << 16) | fps << 8 | !!(params.ip != 'p'))
    {
        case ((A2P_RES_1080 << 16) | (A2P_FPS_25 << 8) | 1):
        case ((A2P_RES_1080 << 16) | (A2P_FPS_29 << 8) | 1):
        case ((A2P_RES_1080 << 16) | (A2P_FPS_30 << 8) | 1):
        case ((A2P_RES_576 << 16) | (A2P_FPS_25 << 8) | 1):
        case ((A2P_RES_480 << 16) | (A2P_FPS_29 << 8) | 1):
            args = (params.ip == 't') ? "--tff" : "--bff";
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
            "--bluray-compat --vbv-maxrate 40000 --vbv-bufsize 30000"
            " --level 4.1 --keyint %d --open-gop --slices 4 %s --sar %s"
            " --colorprim %s --transfer %s --colormatrix %s",
            keyint, args, sar, color, color, color);
}

struct params parse_opts(int argc, char **argv, struct params params)
{
    int parse = 0;
    int index = 0;

    while((parse = getopt_long_only(argc, argv, short_opts, long_opts, &index)) != -1)
    {
        switch(parse) {
            case 'a':
            case 'w':
                params.action = A2P_ACTION_AUDIO;
                if(optarg)
                    params.bit = optarg;
                params.is_raw = (parse == 'a') ? 1 : 0;
                break;

            case 't':
            case 'b':
            case 'p':
                params.action = A2P_ACTION_Y4M;
                if(optarg)
                {
                    sscanf(optarg, "%d:%d", &params.sar_x, &params.sar_y);
                    if(!(((params.sar_x > 0) && (params.sar_y >0)) ||
                          (!params.sar_x && !params.sar_y)))
                    {
                        fprintf(stderr, "invalid arguments.\n");
                        params.action = A2P_ACTION_NOTHING;
                    }
                }
                params.ip = parse;
                break;

            case 'v':
                params.action = A2P_ACTION_PACKEDRAW;
                break;

            case 'i':
                params.action = A2P_ACTION_INFO;
                break;

            case 'x':
                params.action = A2P_ACTION_X264BD;
                params.ip = (strncmp(optarg, "tff", 3)) ? 't' :
                            (strncmp(optarg, "bff", 3)) ? 'b' : 'p';
                break;

            case 'B':
                params.action = A2P_ACTION_BENCHMARK;
                break;

            default:
                params.action = A2P_ACTION_NOTHING;
        }
    }

    return params;
}

void usage(char *binary)
{
    #ifdef A2P_AVS26
        fprintf(stderr, "avs2pipemod for AviSynth 2.6.0 Alpha 2");
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
            "   -audio[=8bit|16bit|24bit|32bit|float  default:unset]\n"
            "       output wav extensible format audio to stdout.\n"
            "       if optional arg is set, bit depth of input will be converted to specified value.\n"
            "\n"
            "   -rawaudio[=8bit|16bit|24bit|32bit|float|mono  default:unset]\n"
            "       output raw pcm audio(without any header) to stdout.\n"
            "       if optional arg is set, input will be converted to specified value.\n"
            "\n"
            "   -y4mp[=sar  default 0:0]\n"
            "       output yuv4mpeg2 format video to stdout as progressive.\n"
            "   -y4mt[=sar  default 0:0]\n"
            "       output yuv4mpeg2 format video to stdout as tff interlaced.\n"
            "   -y4mb[=sar  default 0:0]\n"
            "       output yuv4mpeg2 format video to stdout as bff interlaced.\n"
            "\n"
            "   -packedraw\n"
            "       output rawvideo(without any header) to stdout.\n"
            #ifdef A2P_AVS26
            "       this mode accepts only packed format(RGB32|RGB24|YUY2|Y8).\n"
            #else
            "       this mode accepts only packed format(RGB32|RGB24|YUY2).\n"
            #endif
            "\n"
            "   -x264bd[=tff|bff  default unset(progressive)]\n"
            "       suggest x264 arguments for bluray disc encoding.\n"
            "\n"
            "   -info  - output information about aviscript clip.\n"
            "\n"
            "   -benchmark - do benchmark and output results to stdout.\n"
            "\n"
            "note : in yuv4mpeg2 output mode, RGB input that has 720pix height or more will be converted to YUV with Rec.709 coefficients instead of Rec.601.\n"
            , A2PM_VERSION, A2PM_DATE_OF_BUILD, binary);
    exit(2);
}