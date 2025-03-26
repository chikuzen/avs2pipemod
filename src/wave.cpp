// This is a personal academic project. Dear PVS-Studio, please check it.

// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
/*
 * Copyright (C) 2010-2016 Chris Beswick <chris.beswick@gmail.com>
 *                         Oka Motofumi <chikuzen.mo at gmail dot com>
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

#include "stdlib.h"
#include "utils.h"
#include "wave.h"


static uint32_t get_channel_mask(uint16_t channels)
{
    switch (channels) {
    case 1:
        return WAV_FC;
    case 2:
        return WAV_FL | WAV_FR;
    case 3:
        return WAV_FL | WAV_FR | WAV_BC;
    case 4:
        return WAV_FL | WAV_FR | WAV_BL | WAV_BR;
    case 5:
        return WAV_FL | WAV_FR | WAV_FC | WAV_BL | WAV_BR;
    case 6:
        return WAV_FL | WAV_FR | WAV_FC | WAV_LF | WAV_BL | WAV_BR;
    case 7:
        return WAV_FL | WAV_FR | WAV_FC | WAV_LF | WAV_BL | WAV_BR | WAV_BC;
    case 8:
        return WAV_FL | WAV_FR | WAV_FC | WAV_LF | WAV_BL | WAV_BR | WAV_FLC | WAV_FRC;
    default:
        return 0;
    }
}


WaveRiffHeader::WaveRiffHeader(wave_args_t& a, size_t header_size)
{
    uint32_t fact_samples = static_cast<uint32_t>(a.samples);
    if(a.samples > UINT32_MAX) {
        a2pm_log(LOG_WARNING, "audio sample number over 32bit limit.\n");
        fact_samples = UINT32_MAX;
    }

    int64_t ds64 = fact_samples * a.channels * a.byte_depth;
    int64_t rs64 = ds64 + header_size - sizeof(WaveChunkHeader);
    uint32_t data_size = static_cast<uint32_t>(ds64);
    uint32_t riff_size = static_cast<uint32_t>(rs64);
    if (rs64 > UINT32_MAX) {
        a2pm_log(LOG_WARNING, "audio size over 32bit limit (4GB), clients may truncate audio.\n");
        data_size = UINT32_MAX;
        riff_size = UINT32_MAX;
    }

    riff.header.id      = WAVE_FOURCC("RIFF");
    riff.header.size    = riff_size;
    riff.type           = WAVE_FOURCC("WAVE");

    format.header.id    = WAVE_FOURCC("fmt ");
    format.header.size  = sizeof(format) - sizeof(format.header);
    format.tag          = a.format;
    format.channels     = static_cast<uint16_t>(a.channels);
    format.sample_rate  = a.sample_rate;
    format.byte_rate    = a.channels * a.sample_rate * a.byte_depth;
    format.block_align  = a.channels * a.byte_depth;
    format.bit_depth    = a.byte_depth * 8;
    format.ext_size     = 0;
    data.header.id      = WAVE_FOURCC("data");
    data.header.size    = data_size;
}

WaveRiffExtHeader::WaveRiffExtHeader(wave_args_t& a)
{
    auto wrh = WaveRiffHeader(a, sizeof(WaveRiffExtHeader));

    riff = wrh.riff;
    format = wrh.format;
    data = wrh.data;

    format.tag = WAVE_FORMAT_EXTENSIBLE;
    format.ext_size = sizeof(valid_bits) + sizeof(channel_mask) + sizeof(sub_format);
    format.header.size += format.ext_size;
    valid_bits = a.byte_depth * 8;
    channel_mask = a.channelmask > 0 ? a.channelmask : get_channel_mask(a.channels);

    WaveGuid sf = {
        a.format, 0x0000, 0x0010,
        {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}
    };
    sub_format = sf;

    fact.header.id = WAVE_FOURCC("fact");
    fact.header.size = sizeof(fact) - sizeof(fact.header);
    fact.samples = a.samples > UINT32_MAX ? UINT32_MAX : static_cast<uint32_t>(a.samples);
}

