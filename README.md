# Spectra

Spectra is a desktop Fourier audio engine for harmonic synthesis, spectral visualization, and educational DSP exploration.

Spectra explores how musical timbre emerges from harmonic structure. It lets users generate a tone from harmonic sliders, visualize the resulting waveform and frequency spectrum, and hear the result immediately in a local desktop window.

## Current Status

This version is a C + Raylib desktop app implementing Milestones 1 and 2:

- Desktop window built with Raylib
- Sine wave and additive synthesis generation
- First 16 harmonic amplitude sliders
- Presets for sine, square-like, saw-like, clarinet-like, and bright string-like tones
- ADSR envelope to reduce clicks
- Normalization to avoid clipping
- Local audio playback through Raylib audio
- WAV export for generated tones
- Desktop waveform drawing
- FFT-based frequency spectrum drawing
- Spectral peak detection for the generated tone
- Small DSP test executable

Audio file import, pitch estimation UI, harmonic extraction from samples, additive resynthesis, and spectrogram rendering are planned next milestones.

## How It Works

Generated sound pipeline:

```text
harmonic settings
-> additive synthesis
-> ADSR envelope
-> normalization
-> Raylib sound buffer
-> playback
-> waveform and spectrum visualization
```

Spectra uses the Fourier transform to represent sound as a combination of frequencies. Additive synthesis reverses this idea by summing sine waves at harmonic multiples of a fundamental frequency.

The current spectrum view applies a Hann window before a readable radix-2 FFT implementation. FFT bins are mapped to frequency with:

```text
frequency = binIndex * sampleRate / fftSize
```

## Project Layout

```text
CMakeLists.txt
src/
  main.c
  audio/
    audio_engine.c
    audio_engine.h
  dsp/
    additive_synth.c
    additive_synth.h
    dsp_types.c
    dsp_types.h
    fft.c
    fft.h
    harmonic_analysis.c
    harmonic_analysis.h
    signal_utils.c
    signal_utils.h
    windowing.c
    windowing.h
  ui/
    theme.c
    theme.h
    widgets.c
    widgets.h
tests/
  dsp_tests.c
```

The older root-level `main.c` and `wav.h` are kept as legacy experiment files; the active desktop app entry point is `src/main.c`.

## Run Natively On Windows

The WSL build can open a window through WSLg, but audio may not route to your Windows headphones. For normal Windows audio, build Spectra as a native Windows executable.

Recommended route: Visual Studio Build Tools + vcpkg + CMake.

1. Install Visual Studio 2022 Build Tools with the **Desktop development with C++** workload.
2. Install CMake if it is not already installed.
3. Install Raylib with vcpkg:

```powershell
git clone https://github.com/microsoft/vcpkg C:\vcpkg
C:\vcpkg\bootstrap-vcpkg.bat
C:\vcpkg\vcpkg install raylib:x64-windows
```

4. Open PowerShell in the project folder from Windows, not inside WSL:

```powershell
cd C:\Users\huyus\Projects\C-Spectra
cmake -S . -B build-windows -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake
cmake --build build-windows --config Release
.\build-windows\Release\spectra_desktop.exe
```

The Windows build enables `SPECTRA_RAYLIB_CUSTOM_FRAME_CONTROL` by default because the current vcpkg Raylib package is built with custom frame control. Spectra therefore performs the buffer swap, Windows event polling, and 60 FPS pacing that Raylib's `EndDrawing()` intentionally omits in that configuration.

## Build and Run In WSL/Linux

Install Raylib and GCC for your platform. On Linux/WSL with Raylib available:

```bash
make
make run
```

The WSL/Linux app binary is written to `build/spectra_desktop`.

## Tests

WSL/Linux:

```bash
make test
```

Windows CMake:

```powershell
cmake --build build-windows --target dsp_tests --config Release
.\build-windows\Release\dsp_tests.exe
```

The tests check sine generation length, Hann window values, additive synthesis normalization, synthetic peak detection, and FFT peak detection for a generated 440 Hz sine wave.

## Controls

- Press **Play tone** or the spacebar to hear the current tone.
- Use the fundamental, duration, and master gain sliders to shape the generated sound.
- Select a preset to load a harmonic profile.
- Adjust harmonic sliders to directly edit timbre.
- Press **Export WAV** to write `spectra-tone.wav`.

## Technical Concepts

- Fourier transform
- Fast Fourier transform (FFT)
- Hann windowing
- Harmonic series
- Additive synthesis
- ADSR envelope
- Spectral peak detection

## Limitations

Spectra is an educational DSP project, not a commercial pitch detector or production audio engine.

Current limitations:

- The app synthesizes tones but does not yet import user audio.
- Spectrum analysis uses a single windowed frame, not a streaming STFT.
- Pitch estimation and harmonic extraction from real audio are not exposed yet.
- The FFT implementation is intentionally readable and dependency-free, not highly optimized.
- Spectra does not use AI or machine learning.
- Spectra does not perfectly identify instruments or timbre.

## Roadmap

- Milestone 3: add pitch estimation for generated and imported tones.
- Milestone 4: add desktop file loading for WAV samples and mono conversion.
- Milestone 5: extract harmonic amplitudes and resynthesize an approximation with additive synthesis.
- Milestone 6: add STFT spectrogram rendering and polish packaging instructions.
