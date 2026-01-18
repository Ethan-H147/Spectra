#include <stdio.h>
#include <stdlib.h>
#include "wav.h" // Import our map

int main() {
    // 1. Open the file
    // "rb" means Read Binary. Essential for audio files!
    FILE *file = fopen("file_example_WAV_1MG.wav", "rb");
    
    if (file == NULL) {
        printf("Error: Could not open file. Did you drag it into the folder?\n");
        return 1;
    }

    // 2. Create a place to store the header info
    WAVHeader header;

    // 3. Read exactly 44 bytes (sizeof header) from file into our header variable
    // fread(destination, size_of_chunk, number_of_chunks, source_file)
    fread(&header, sizeof(WAVHeader), 1, file);

    // 4. Close the file (Good manners!)
    fclose(file);

    // 5. Print the results to prove we read it correctly
    printf("--- WAV File Analysis ---\n");
    printf("Sample Rate: %u Hz\n", header.sample_rate);
    printf("Channels:    %u\n", header.num_channels);
    printf("Bit Depth:   %u bits\n", header.bit_depth);
    
    // 6. Check if it's CD Quality (44100Hz)
    if (header.sample_rate == 44100) {
        printf("Quality:     CD Standard\n");
    } else {
        printf("Quality:     Other\n");
    }

    return 0;
}