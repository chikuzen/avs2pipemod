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

#include <stdlib.h>
#include "common.h"
#include "wave.h"

void
wave_guid_copy(WaveGuid *dst, WaveGuid *src)
{
    dst->d1 = src->d1;
    dst->d2 = src->d2;
    dst->d3 = src->d3;
    dst->d4[0] = src->d4[0];
    dst->d4[1] = src->d4[1];
    dst->d4[2] = src->d4[2];
    dst->d4[3] = src->d4[3];
    dst->d4[4] = src->d4[4];
    dst->d4[5] = src->d4[5];
    dst->d4[6] = src->d4[6];
    dst->d4[7] = src->d4[7];
}

WaveRiffHeader *
wave_create_riff_header(WaveFormatType format,
                        uint16_t       channels,
                        uint32_t       sample_rate,
                        uint16_t       byte_depth,
                        uint64_t       samples)
{
    WaveRiffHeader *header = malloc(sizeof(*header));
    WaveGuid sub_format = {WAVE_FORMAT_PCM, 0x0000, 0x0010,
                            {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71} };
    uint32_t data_size, riff_size, fact_samples;

    sub_format.d1 = format;
    wave_guid_copy(&header->format.sub_format, &sub_format);

    if(samples > UINT32_MAX) {
        a2p_log(A2P_LOG_WARNING, "audio sample number over 32bit limit.\n");
        fact_samples = UINT32_MAX;
    } else {
        fact_samples = (uint32_t) samples;
    }

    if((samples * channels * byte_depth + sizeof(*header) 
          - sizeof(header->riff.header)) > UINT32_MAX) {
        a2p_log(A2P_LOG_WARNING, "audio size over 32bit limit (4GB)"
                                 ", clients may truncate audio.\n");
        data_size = UINT32_MAX;
        riff_size = UINT32_MAX;
    } else {
        data_size = fact_samples * channels * byte_depth;
        riff_size = data_size + sizeof(*header) - sizeof(header->riff.header);
    }

    header->riff.header.id      = WAVE_FOURCC('R', 'I', 'F', 'F');
    header->riff.header.size    = riff_size;
    header->riff.type           = WAVE_FOURCC('W', 'A', 'V', 'E');

    header->format.header.id    = WAVE_FOURCC('f', 'm', 't', ' ');
    header->format.header.size  = sizeof(header->format) 
                                    - sizeof(header->format.header);
    header->format.tag          = WAVE_FORMAT_EXTENSIBLE;
    header->format.channels     = channels;
    header->format.sample_rate  = sample_rate;
    header->format.byte_rate    = channels * sample_rate * byte_depth;
    header->format.block_align  = channels * byte_depth;
    header->format.bit_depth    = byte_depth * 8;
    header->format.ext_size     = sizeof(header->format.valid_bits)
                                    + sizeof(header->format.channel_mask)
                                    + sizeof(header->format.sub_format);
    header->format.valid_bits   = byte_depth * 8;
    header->format.channel_mask = 0;

    header->fact.header.id      = WAVE_FOURCC('f', 'a', 'c', 't');
    header->fact.header.size    = sizeof(header->fact)
                                    - sizeof(header->fact.header);
    header->fact.samples        = fact_samples;
    header->data.header.id      = WAVE_FOURCC('d', 'a', 't', 'a');
    header->data.header.size    = data_size;

    return header;
}

WaveRf64Header *
wave_create_rf64_header(WaveFormatType format,
                        uint16_t       channels,
                        uint32_t       sample_rate,
                        uint16_t       byte_depth,
                        uint64_t       samples)
{
    WaveRf64Header *header = malloc(sizeof(*header));
    WaveGuid sub_format = {WAVE_FORMAT_PCM, 0x0000, 0x0010,
                            {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71} };

    sub_format.d1 = format;
    wave_guid_copy(&header->format.sub_format, &sub_format);

    header->riff.header.id      = WAVE_FOURCC('R', 'F', '6', '4');
    header->riff.header.size    = -1;
    header->riff.type           = WAVE_FOURCC('W', 'A', 'V', 'E');

    header->ds64.header.id      = WAVE_FOURCC('d', 's', '6', '4');
    header->ds64.header.size    = sizeof(header->ds64)
                                    - sizeof(header->ds64.header);
    header->ds64.riff_size      = samples * channels * byte_depth
                                    + sizeof(*header)
                                    - sizeof(header->riff.header);
    header->ds64.data_size      = samples * channels * byte_depth;
    header->ds64.fact_samples   = samples;
    header->ds64.table_size     = 0;

    header->format.header.id    = WAVE_FOURCC('f', 'm', 't', ' ');
    header->format.header.size  = sizeof(header->format) 
                                    - sizeof(header->format.header);
    header->format.tag          = WAVE_FORMAT_EXTENSIBLE;
    header->format.channels     = channels;
    header->format.sample_rate  = sample_rate;
    header->format.byte_rate    = channels * sample_rate * byte_depth;
    header->format.block_align  = channels * byte_depth;
    header->format.bit_depth    = byte_depth * 8;
    header->format.ext_size     = sizeof(header->format.valid_bits)
                                    + sizeof(header->format.channel_mask)
                                    + sizeof(header->format.sub_format);
    header->format.valid_bits   = byte_depth * 8;
    header->format.channel_mask = 0;

    header->fact.header.id      = WAVE_FOURCC('f', 'a', 'c', 't');
    header->fact.header.size    = sizeof(header->fact)
                                    - sizeof(header->fact.header);
    header->fact.samples        = -1;
    header->data.header.id      = WAVE_FOURCC('d', 'a', 't', 'a');
    header->data.header.size    = -1;

    return header;
}
