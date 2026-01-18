#ifndef WAV_H
#define WAV_H

#include <stdint.h> // Allows us to use specific byte sizes like uint32_t

// This struct matches the first 44 bytes of a standard WAV file exactly.
typedef struct {
    char riff_header[4];    // Contains "RIFF"
    uint32_t wav_size;      // Size of the wav portion of the file
    char wave_header[4];    // Contains "WAVE"
    char fmt_header[4];     // Contains "fmt " (includes trailing space)
    uint32_t fmt_chunk_size;// Should be 16 for PCM
    uint16_t audio_format;  // Should be 1 for PCM. 3 for IEEE Float
    uint16_t num_channels;  // 1 = Mono, 2 = Stereo
    uint32_t sample_rate;   // e.g., 44100
    uint32_t byte_rate;     // Number of bytes per second
    uint16_t sample_alignment; // num_channels * bytes_per_sample
    uint16_t bit_depth;     // Number of bits per sample (16, 24, 32)
    char data_header[4];    // Contains "data"
    uint32_t data_bytes;    // Number of bytes in data. Number of samples * num_channels * sample byte size
} WAVHeader;

#endif