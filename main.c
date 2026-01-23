#include <stdio.h>
#include <stdlib.h>
#include "wav.h" // Import our map
#include "raylib.h" // Import the graphics library

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600

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
    // 11. VISUALIZE THE WAVEFORM USING RAYLIB
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "C-Spectra - Waveform Viewer");
    SetTargetFPS(60);

    // --- NEW: AUDIO SETUP ---
    InitAudioDevice(); // Turn on your Alienware's audio driver
    Music music = LoadMusicStream("file_example_WAV_1MG.wav"); // Tell Raylib to stream this file
    music.looping = false; // Stop it from restarting indefinitely
    PlayMusicStream(music); // Start the audio engine

    // Calculate total 16-bit samples (Total Bytes / 2)
    int total_samples = header.data_bytes / 2;

    // 1. Create a "cursor" that remembers where we are in the song
    int current_sample_index = 0; 

    while (!WindowShouldClose()) {
        
        // --- UPDATE LOGIC (The Physics) ---
        // Every frame, move the cursor forward by 735 samples.
        // This simulates "playing" the song at real-time speed (44100 / 60 = 735).
        if (IsKeyDown(KEY_RIGHT)) {
            current_sample_index += 2000; // Fast forward if you hold Right Arrow
        } else {
            current_sample_index += 735;  // Normal play speed
        }

        // Loop the song: If we hit the end, go back to start
        if (current_sample_index >= total_samples) {
            current_sample_index = 0;
        }

        // --- DRAW LOGIC (The Painting) ---
        BeginDrawing();
        ClearBackground(BLACK);

        DrawLine(0, SCREEN_HEIGHT/2, SCREEN_WIDTH, SCREEN_HEIGHT/2, DARKGRAY);
        DrawText("Status: PLAYING (Arrow Keys to Seek)", 10, 10, 20, GREEN);

        // Draw the wave starting from our moving cursor
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            
            // Safety: Stop drawing if we reach the end of the buffer
            if (current_sample_index + x >= total_samples) break;

            short sample_value = ((short*)buffer)[current_sample_index + x];

            // Scaling: (Sample / 100)
            float y = (SCREEN_HEIGHT / 2) + (sample_value / 100.0f);
            
            // Draw connected lines instead of dots for a smoother look
            if (x > 0) {
                 // Connect previous pixel to current pixel
                 short prev_val = ((short*)buffer)[current_sample_index + x - 1];
                 float prev_y = (SCREEN_HEIGHT / 2) + (prev_val / 100.0f);
                 DrawLine(x-1, (int)prev_y, x, (int)y, LIME);
            }
        }
        
        EndDrawing();
    }
    // 12. CLEAN UP (Crucial!)
    free(buffer);   // Give the memory back to the OS
    fclose(file);   // Now we can finally hang up the phone

    // --- WEEK 3 ENDS HERE ---
    
    return 0;
}