// This file is part of avs2pipemod.

#ifdef A2P_AVS26
    #include "avisynth26/avisynth_c.h"
#else
    #include "avisynth25/avisynth_c.h"
#endif

static const AVS_VideoInfo * avisynth_filter(AVS_Clip *clip, AVS_ScriptEnvironment *env, const AVS_VideoInfo *info, const char *filter)
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
static const AVS_VideoInfo * avisynth_filter_yuv2yuv(AVS_Clip *clip, AVS_ScriptEnvironment *env, const AVS_VideoInfo *info, const char *filter, a2pm_bool is_interlaced)
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

static const AVS_VideoInfo * avisynth_filter_rgb2yuv(AVS_Clip *clip, AVS_ScriptEnvironment *env, const AVS_VideoInfo *info, const char *filter, const char *matrix, a2pm_bool is_interlaced)
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

static const AVS_VideoInfo * avisynth_filter_trim(AVS_Clip *clip, AVS_ScriptEnvironment *env, const AVS_VideoInfo *info, int start_frame, int end_frame)
{
    AVS_Value val_clip = avs_new_value_clip(clip);
    avs_release_clip(clip);

    AVS_Value val_array[3] =
        {val_clip, avs_new_value_int(start_frame), avs_new_value_int(end_frame)};
    AVS_Value val_return =
        avs_invoke(env, "Trim", avs_new_value_array(val_array, 3), 0);
    if(avs_is_error(val_return))
        a2p_log(A2P_LOG_ERROR, "Trim failed.\n");

    clip = avs_take_clip(val_return, env);
    avs_release_value(val_clip);
    avs_release_value(val_return);

    a2p_log(A2P_LOG_INFO, "set frame number %d as first frame.\n", start_frame);
    return avs_get_video_info(clip);
}

static AVS_Clip * avisynth_source(char *file, AVS_ScriptEnvironment *env)
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
