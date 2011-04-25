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

#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

enum A2pLogLevel {
    A2P_LOG_ERROR,
    A2P_LOG_WARNING,
    A2P_LOG_INFO,
    A2P_LOG_REPEAT
};

void a2p_log(int level, const char *message, ...);

//double a2pm_gettime(void);

int64_t a2pm_gettime(void);

#endif // COMMON_H
