// This file is part of avs2pipemod.

#define MAX_NUM_PLANES 3

static int64_t a2pm_gettime(void)
{
    struct timeb tb;
    ftime( &tb );
    return ((int64_t)tb.time * 1000 + (int64_t)tb.millitm) * 1000;
}

static char * pix_type_to_string(int pix_type, a2pm_bool is_info)
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

#ifdef A2P_AVS26
static void a2pm_csp_handle26(int pix_type, yuvout *data)
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

static int a2pm_write_planar_frames(AVS_Clip *clip, AVS_ScriptEnvironment *env, const AVS_VideoInfo *info, yuvout *yuvout, a2pm_bool is_raw)
{
    const int planes[] = {AVS_PLANAR_Y, AVS_PLANAR_U, AVS_PLANAR_V};

    int width[MAX_NUM_PLANES], height[MAX_NUM_PLANES], buff_inc[MAX_NUM_PLANES];
    for(int p = 0; p < yuvout->num_planes; p++) {
        width[p] = info->width >> (p ? yuvout->h_uv : 0);
        height[p] = info->height >> (p? yuvout->v_uv : 0);
        buff_inc[p] = width[p] * height[p];
    }

    size_t count = ((info->width * info->height * avs_bits_per_pixel(info)) >> 3)
                   + (is_raw ? 0 : Y4M_FRAME_HEADER_SIZE);

    BYTE *buff = (BYTE*)malloc(count);
    if (!buff)
        a2p_log(A2P_LOG_ERROR, "malloc failed.\n", 0);

    BYTE *buff_0 = buff;
    if (!is_raw) {
        memcpy(buff, Y4M_FRAME_HEADER_STRING, Y4M_FRAME_HEADER_SIZE);
        buff_0 += Y4M_FRAME_HEADER_SIZE;
    }

    int wrote;
    for(wrote = 0; wrote < info->num_frames; wrote++) {
        AVS_VideoFrame *frame = avs_get_frame(clip, wrote);
        BYTE *buff_ptr = buff_0;
        for(int p = 0; p < yuvout->num_planes; p++) {
            avs_bit_blt(env, buff_ptr, width[p], avs_get_read_ptr_p(frame, planes[p]),
                        avs_get_pitch_p(frame, planes[p]), width[p], height[p]);
            buff_ptr += buff_inc[p];
        }
        size_t step = fwrite(buff, sizeof(BYTE), count, stdout);
        avs_release_frame(frame);
        if(step != count)
            break;
    }
    free(buff);
    return wrote;
}

static int a2pm_write_packed_frames(AVS_Clip *clip, AVS_ScriptEnvironment *env, const AVS_VideoInfo *info)
{
    int width = info->width * avs_bits_per_pixel(info) >> 3;
    size_t count = width * info->height;

    BYTE *buff = (BYTE*)malloc(count);
    if (!buff)
        a2p_log(A2P_LOG_ERROR, "malloc failed.\n");

    int wrote = 0;
    for(wrote = 0; wrote < info->num_frames; wrote++) {
        AVS_VideoFrame *frame = avs_get_frame(clip, wrote);
        avs_bit_blt(env, buff, width, avs_get_read_ptr(frame), avs_get_pitch(frame), width, info->height);
        size_t step = fwrite(buff, sizeof(BYTE), count, stdout);
        avs_release_frame(frame);
        if(step != count)
            break;
    }
    free(buff);
    return wrote;
}

static void do_audio(AVS_Clip *clip, AVS_ScriptEnvironment *env, params *params)
{
    const AVS_VideoInfo *info = avs_get_video_info(clip);

    if(!avs_has_audio(info))
        a2p_log(A2P_LOG_ERROR, "clip has no audio.\n", 0);

    if(avs_has_video(info) && (params->start_frame || params->end_frame))
        info = avisynth_filter_trim(clip, env, info, params->start_frame, params->end_frame);

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
        if (params->fmt_type == A2PM_EXTRA2) {
            WaveRf64Header *header
                = wave_create_rf64_header(format, info->nchannels,
                                          info->audio_samples_per_second,
                                          avs_bytes_per_channel_sample(info),
                                          info->num_audio_samples);
            fwrite(header, sizeof(*header), 1, stdout);
            free(header);
        } else if (params->fmt_type == A2PM_EXTRA) {
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

static void do_y4m(AVS_Clip *clip, AVS_ScriptEnvironment *env, params *params)
{
    const AVS_VideoInfo *info = avs_get_video_info(clip);

    if(!avs_has_video(info))
        a2p_log(A2P_LOG_ERROR, "clip has no video.\n");

    if(params->start_frame || params->end_frame)
        info = avisynth_filter_trim(clip, env, info, params->start_frame, params->end_frame);

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
        if(info->width & 1 || info->height & 1 || (params->ip != 'p' && info->height & 3))
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

    //setvbuf(stdout, NULL, _IOFBF, BUFSIZE_OF_STDOUT);

    int64_t start = a2pm_gettime();

    // YUV4MPEG2 stream header http://linux.die.net/man/5/yuv4mpeg
    fprintf(stdout, "YUV4MPEG2 W%d H%d F%u:%u I%c A%d:%d C%s\n", info->width,
            info->height, info->fps_numerator, info->fps_denominator,
            params->ip, params->sar_x, params->sar_y, y4mout.y4m_ctag_value);

    int wrote = a2pm_write_planar_frames(clip, env, info, &y4mout, A2PM_FALSE);

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

static void do_rawvideo(AVS_Clip *clip, AVS_ScriptEnvironment *env, params *params)
{
    const AVS_VideoInfo *info = avs_get_video_info(clip);
    yuvout rawout = {3, 1, 1, ""};

    if(!avs_has_video(info))
        a2p_log(A2P_LOG_ERROR, "clip has no video.\n");

    if(params->start_frame || params->end_frame)
        info = avisynth_filter_trim(clip, env, info, params->start_frame, params->end_frame);

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

    //setvbuf(stdout, NULL, _IOFBF, BUFSIZE_OF_STDOUT);

    int64_t start = a2pm_gettime();

    int wrote = is_planar ?
                a2pm_write_planar_frames(clip, env, info, &rawout, A2PM_TRUE) :
                a2pm_write_packed_frames(clip, env, info);

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

static void do_info(AVS_Clip *clip, AVS_ScriptEnvironment *env, char *input, a2pm_bool need_audio)
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

static void do_benchmark(AVS_Clip *clip, AVS_ScriptEnvironment *env, char *input, params *params)
{
    const AVS_VideoInfo *info = avs_get_video_info(clip);

    if(!avs_has_video(info))
        a2p_log(A2P_LOG_ERROR, "clip has no video.\n");

    do_info(clip, env, input, A2PM_FALSE);

    if(params->start_frame || params->end_frame)
        info = avisynth_filter_trim(clip, env, info, params->start_frame, params->end_frame);

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

static void do_x264bd(AVS_Clip *clip, AVS_ScriptEnvironment *env, params *params)
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

