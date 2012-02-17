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

#ifndef AVS2PIPEMOD_H
#define AVS2PIPEMOD_H

#include <stdint.h>

#define A2PM_VERSION "0.3.0"

#define RETURN_IF_ERROR(cond, ret, ...) \
    if (cond) {a2pm_log(A2PM_LOG_ERROR, __VA_ARGS__); return ret;}


typedef enum {
    A2PM_ACT_AUDIO,
    A2PM_ACT_VIDEO,
    A2PM_ACT_INFO,
    A2PM_ACT_X264BD,
    A2PM_ACT_BENCHMARK,
    A2PM_ACT_X264RAW,
    A2PM_ACT_DUMP_YUV_AS_TXT,
    A2PM_ACT_NOTHING
} action_t;

typedef enum {
    A2PM_FMT_RAWVIDEO,
    A2PM_FMT_RAWVIDEO_VFLIP,
    A2PM_FMT_YUV4MPEG2,
    A2PM_FMT_RAWAUDIO,
    A2PM_FMT_WAVEFORMATEX,
    A2PM_FMT_WAVEFORMATEXTENSIBLE,
 //   A2PM_FMT_RF64,
    A2PM_FMT_HDBD,
    A2PM_FMT_SDBD,
    A2PM_FMT_WITHOUT_TCFILE,
    A2PM_FMT_WITH_TCFILE,
    A2PM_FMT_NOTHING
} format_type_t;

typedef struct {
    action_t      action;
    format_type_t format_type;
    int           sar[2];
    int           trim[2];
    char         *input;
    char          frame_type;
    char         *bit;
    int           yuv_depth;
} params_t;

typedef enum {
    A2PM_LOG_INFO,
    A2PM_LOG_ERROR,
    A2PM_LOG_WARNING,
    A2PM_LOG_REPEAT
} log_level_t;

void a2pm_log(log_level_t level, const char *message, ...);

#endif /* AVS2PIPEMOD_H */