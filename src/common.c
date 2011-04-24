/* 
 * Copyright (C) 2010 Chris Beswick <chris.beswick@gmail.com>
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

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include "common.h"

void a2p_log(int level, const char *message, ...)
{
    char *prefix;
    va_list args;

    va_start(args, message);

    switch(level)
    {
        case A2P_LOG_ERROR:
            prefix = "error";
            break;
        case A2P_LOG_WARNING:
            prefix = "warn";
            break;
        case A2P_LOG_INFO:
        case A2P_LOG_REPEAT:
            prefix = "info";
            break;
        default:
            prefix = "unknown";
            break;
    }

    if(level == A2P_LOG_REPEAT) fprintf(stderr, "\r");
    fprintf(stderr, "avs2pipemod [%s]: ", prefix);
    vfprintf(stderr, message, args);

    va_end(args);
    if(level == A2P_LOG_ERROR) exit(2);
}

double a2pm_gettime()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec * 1e-6;
}
