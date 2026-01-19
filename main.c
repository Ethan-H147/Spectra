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

    // --- WEEK 3 NEW CODE STARTS HERE ---

    // 7. Calculate how much memory we need
    // The header tells us exactly how big the audio data is (in bytes)
    printf("Audio Data Size: %u bytes\n   %.2f KB\n", header.data_bytes, header.data_bytes / 1024.0);

    // 8. Allocate memory on the HEAP (malloc)
    // We create a pointer 'buffer' that points to our new storage room.
    unsigned char *buffer = malloc(header.data_bytes); 

    // Safety Check: Did the OS give us the room?
    if (buffer == NULL) {
        printf("Error: Not enough memory!\n");
        return 1;
    }

    // 9. Read the data
    // fread(destination, size_of_one_byte, how_many_bytes, source_file)
    fread(buffer, 1, header.data_bytes, file);

    // 10. Verify: Print the first few bytes to see the raw data
    printf("First 5 bytes of data: ");
    for (int i = 1; i < 10; i++) {
        printf("%02x ", buffer[i]); // %02x prints hex numbers (like FF, 0A)
    }
    printf("\n");

    // 11. CLEAN UP (Crucial!)
    free(buffer);   // Give the memory back to the OS
    fclose(file);   // Now we can finally hang up the phone

    // --- WEEK 3 ENDS HERE ---
    
    return 0;
}