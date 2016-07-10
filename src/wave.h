/* 
 * Copyright (C) 2010-2016 Oka Motofumi <chikuzen.mo at gmail dot com>
 *                         Chris Beswick <chris.beswick@gmail.com>
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

// Wave File Specifications 
// http://www-mmsp.ece.mcgill.ca/documents/audioformats/wave/wave.html

#ifndef WAVE_H
#define WAVE_H

#include <stdint.h>




static inline uint32_t WAVE_FOURCC(const char* str)
{
    return *reinterpret_cast<const uint32_t*>(str);
}


enum WaveFormatType : uint32_t {
    WAVE_FORMAT_PCM         = 0x0001,   // samples are ints
    WAVE_FORMAT_IEEE_FLOAT  = 0x0003,   // samples are floats
    WAVE_FORMAT_EXTENSIBLE  = 0xFFFE    // not a real type.
};


enum speaker_position : uint32_t {
    WAV_FL  = 0x0001,
    WAV_FR  = 0x0002,
    WAV_FC  = 0x0004,
    WAV_LF  = 0x0008,
    WAV_BL  = 0x0010,
    WAV_BR  = 0x0020,
    WAV_FLC = 0x0040,
    WAV_FRC = 0x0080,
    WAV_BC  = 0x0100,
    WAV_SL  = 0x0200,
    WAV_SR  = 0x0400,
};


struct wave_args_t {
    WaveFormatType format;
    int channels;
    int sample_rate;
    int byte_depth;
    int64_t samples;
};


// set packing alignment to 1 byte so we can just fwrite structs
// gcc docs say it supports this to be compatable with vs.
#pragma pack(push, 1)

// docs state a chunk should be [id, size, [data, ...]] but this way 
// means I can get size = sizeof(chunk) - sizeof(chunk.header)

// just a uuid, but this works so...
struct WaveGuid {
    uint32_t d1;
    uint16_t d2;
    uint16_t d3;
    uint8_t  d4[8];
};

// really RiffChunkHeader, but Wave* naming seems neater
struct WaveChunkHeader {            
    uint32_t        id;             // FOURCC
    uint32_t        size;           // size of chunk data
};

// first riff chunk at the start of the file
struct WaveRiffChunk {
    WaveChunkHeader header;         // id = RIFF, size = total size - sizeof(header)
    uint32_t        type;           // WAVE
};

// data size chunk for RF64 format
struct WaveDs64Chunk {
    WaveChunkHeader header;         // id = ds64, size = sizeof(WaveRf64Chunk) - sizeof(header)
    uint64_t        riff_size;      // replaces WaveRiffHeader.size when latter = -1
    uint64_t        data_size;      // replaces WaveDataHeader.size when latter = -1
    uint64_t        fact_samples;   // replaces WaveFactChunk.samples when latter = -1
    uint32_t        table_size;     // 0, spec does not say what this is for, anyone???
};

// wave format chunk based on WAVE_FORMAT
struct WaveFormatChunk {
    WaveChunkHeader header;
    uint16_t        tag;
    uint16_t        channels;
    uint32_t        sample_rate;
    uint32_t        byte_rate;
    uint16_t        block_align;
    uint16_t        bit_depth;
    uint16_t        ext_size;
};


struct WaveFormatExtChunk {
    uint16_t        valid_bits;     // equal to bit_depth if uncompressed
    uint32_t        channel_mask;   // speaker position mask
    WaveGuid        sub_format;     // guid of sub format eg. pcm, float, ...
};

// wave fact chunk for WAVE_FORMAT_EXTENSIBLE, required when not PCM, always used in this imp
struct WaveFactChunk {
    WaveChunkHeader header;         // id = FACT, size = sizeof(WaveFactChunk) - sizeof(header)
    uint32_t        samples;        // number of channels * total samples per channel
};

// partial data chunk, just the header, would be stupid to place ALL data into a struct
struct WaveDataChunk {
    WaveChunkHeader header;         // id = FACT, size = sizeof(data)
};

// RIFF header for a WAVE_FORMAT file
struct WaveRiffHeader {
    WaveRiffChunk   riff;
    WaveFormatChunk format;
    WaveDataChunk   data;
    WaveRiffHeader(wave_args_t& a, size_t header_size=sizeof(WaveRiffHeader));
};

// complete RIFF header for a WAVE_FORMAT_EXTENSIBLE file
struct WaveRiffExtHeader {
    WaveRiffChunk   riff;
    WaveFormatChunk format;
    uint16_t valid_bits;
    uint32_t channel_mask;
    WaveGuid sub_format;
    WaveFactChunk   fact;
    WaveDataChunk   data;
    WaveRiffExtHeader(wave_args_t& a);
};


// pop previous packing alignment
#pragma pack(pop)


#endif // WAVE_H
