/* 
 * Copyright (C) 2010 Chris Beswick <chris.beswick@gmail.com>
 *               2011-2012 Oka Motofumi <chikuzen.mo at gmail dot com>
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

#include <stdarg.h>
#include <stdio.h>

#include "avs2pipemod.h"

void a2pm_log(log_level_t level, const char *message, ...)
{
    char *prefix;
    va_list args;

    va_start(args, message);

    switch(level) {
    case A2PM_LOG_ERROR:
        prefix = "error";
        break;
    case A2PM_LOG_WARNING:
        prefix = "warning";
        break;
    case A2PM_LOG_INFO:
    case A2PM_LOG_REPEAT:
        prefix = "info";
        break;
    default:
        prefix = "unknown";
        break;
    }

    if(level == A2PM_LOG_REPEAT)
        fprintf(stderr, "\r");
    fprintf(stderr, "avs2pipemod[%s]: ", prefix);
    vfprintf(stderr, message, args);

    va_end(args);
}