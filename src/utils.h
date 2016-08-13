/*
* Copyright (C) 2016 Oka Motofumi <chikuzen.mo at gmail dot com>
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


#ifndef A2PM_UTILS_H
#define A2PM_UTILS_H

#include <cstdarg>
#include <stdexcept>


void validate(bool cond, const char* message, ...);


enum {
    LOG_INFO,
    LOG_REPEAT,
    LOG_WARNING,
};

void a2pm_log(int level, const char* message, ...);

const char* get_string_info(int pix_type);

const char* get_string_video_out(int pix_type);

const char* get_string_y4mheader(int pix_type);

int get_sample_bits(int pixel_type);

int get_num_planes(int pixel_type);

//const char* get_string_filter(int pix_type);

class Buffer {
    void* buff;
public:
    Buffer(size_t size, size_t align = 16);
    ~Buffer();
    void* data();
};

#endif

