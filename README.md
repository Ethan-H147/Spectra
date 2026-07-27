# Spectra

Spectra is a C99 desktop application for studying Fourier representations of audio. Raylib supplies the window, input, audio device, and decoded audio samples.

The application generates tones, imports audio files, analyzes selected regions, and reconstructs audio from reduced spectral representations. Each reconstruction path models a different mathematical object.

Spectra keeps these terms distinct:

- A **harmonic** is an integer multiple of an estimated fundamental frequency.
- An **FFT bin** is one sample of a discrete Fourier transform.
- A **Fourier component** stores one complex FFT coefficient and its bin index.
- An **STFT frame** is a windowed time segment with its own Fourier components.
- A **reconstruction** is an audio buffer produced from a selected spectral representation.

The active application entry point is `src/main.c`. The root-level `main.c` and `wav.h` belong to an earlier experiment.

## Reconstruction models

| Model | Analysis scope | Selection unit | Phase | Output |
| --- | --- | --- | --- | --- |
| Additive synthesizer | Generated tone | 16 integer harmonics | Zero initial oscillator phase | Mono |
| Harmonic resynthesis | Selected pitched region | Detected integer harmonics | Discarded | Mono |
| Fourier frame | One centered Hann frame | Ranked complex Fourier components | Preserved | Mono frame |
| Mono FFT | Complete audio file | Fixed one-sided FFT bins | Preserved | Mono |
| Stereo FFT | Complete left and right channels | Fixed FFT bins per channel | Preserved | Stereo |
| STFT | Overlapping frames | FFT bins per frame and channel | Preserved | Source channel layout |

The component counts from these models are not interchangeable. A Top-5 fixed FFT uses five frequencies for the complete file. A Top-5 STFT selects five frequencies in every frame and channel.

Harmonic resynthesis also differs from both Fourier modes. It matches peaks near integer multiples of a detected fundamental. The model does not retain the original Fourier phase.

## Signal processing

### Additive synthesis

The synthesizer renders 16 sinusoidal harmonics at a 44.1 kHz sample rate. Each harmonic has an independent amplitude.

Five presets define initial harmonic profiles:

- Sine
- Square-like
- Saw-like
- Clarinet-like
- Bright string

The renderer applies an ADSR envelope and normalizes the result before playback. The workspace displays the generated waveform, magnitude spectrum, spectral peaks, and pitch estimate.

### Audio import

Spectra imports WAV, MP3, OGG, and FLAC files. The import layer reads files through UTF-8-aware platform functions.

Raylib decodes the file from memory. Spectra then stores two representations:

1. An interleaved playback buffer that preserves mono or stereo channels.
2. A mono analysis reference for pitch, waveform, spectrum, and spectrogram calculations.

Sources with more than two channels produce a mono playback and analysis buffer. Spectra does not define a surround-channel mapping.

Full-file reconstruction rejects audio longer than 600 seconds.

### Region analysis

Audio Analysis selects a region from the mono analysis reference. The analysis path computes:

- A 2,048-bin waveform summary for display.
- A Hann-windowed magnitude spectrum.
- Up to 64 local spectral peaks.
- A YIN-style periodicity estimate.
- A musical note and cents offset.

Spectrum analysis uses at most 16,384 centered samples from the selected region. Pitch analysis uses the complete selected region.

### Integer-harmonic resynthesis

The harmonic path requires a valid pitch estimate with at least 0.60 confidence. It searches for spectral peaks near the first 24 integer multiples.

The resynthesizer creates stationary sinusoids from the matched amplitudes. It does not reproduce transients, broadband noise, phase relationships, or time-varying articulation.

### Fourier-frame reconstruction

The frame path analyzes one centered, windowed FFT frame. It ranks the complex one-sided Fourier coefficients by magnitude.

Inverse reconstruction restores each selected coefficient with its conjugate mirror. This path preserves phase within the frame.

The output represents one short frame. It does not represent the complete audio file.

### Fixed whole-file FFT

The fixed model zero-pads the complete source to the next power-of-two length. It computes one radix-2 FFT for each modeled channel.

The ranker includes every independent one-sided FFT bin:

- DC at bin 0.
- Positive-frequency bins.
- Nyquist at the final independent bin.

The inverse transform restores conjugate mirror bins for real-valued output. Spectra trims the resulting buffer to the original frame count.

Mono FFT analyzes the mono analysis reference. Stereo FFT builds separate models for the preserved left and right channels.

The fixed model supports two selection methods:

1. **FFT-bin count** selects an exact number of ranked bins.
2. **Spectral energy** selects the smallest count that reaches an energy target.

Stereo energy selection uses one shared count. That count reaches the target in each channel.

Zero padding reduces the spacing between FFT-bin frequencies. It does not add source information or improve the original frequency resolution.

### Time-varying STFT

The STFT path uses a 2,048-sample Hann window and a 512-sample hop. It centers each frame with half-window padding.

Each frame ranks 1,023 positive-frequency bins between DC and Nyquist. The selected count applies independently to every frame and channel.

The inverse path restores conjugate symmetry and runs an inverse FFT. Normalized overlap-add combines the frames.

This model represents time-varying frequency content. Its count defines a per-frame budget, not a complete-file budget.

### Spectrogram

Spectra computes a visualization-only STFT from the mono analysis reference. The texture contains 256 time bins and 128 frequency bins.

The displayed frequency range ends at 12 kHz or the source Nyquist frequency. Pixel values store decibels from the frame magnitude spectrum.

The visualization does not select reconstruction bins. Mono FFT, Stereo FFT, and STFT use separate reconstruction state.

## Memory and concurrency

### Fixed-model memory policy

The default fixed-model limit equals 768 MB. Settings cycles through 512 MB, 768 MB, 1 GB, and 1.5 GB.

The estimator counts transform arrays and ranked Fourier components. The limit governs fixed-model allocations, not total process memory.

For stereo sources, startup selects the largest channel configuration that fits:

1. Stereo FFT when two models fit.
2. Mono FFT when only one model fits.
3. No fixed model when one model exceeds the limit.

STFT remains available when the fixed model exceeds the limit. Changing the memory limit does not rebuild a model that already exists.

### Background tasks

Full-file DSP runs outside the UI thread:

- Spectrogram generation.
- Fixed whole-file FFT analysis.
- Fixed whole-file inverse reconstruction.
- STFT inverse reconstruction.

`src/platform/background_task.c` uses a Windows thread or a POSIX thread. A lock protects cancellation, completion, and progress state.

The UI thread joins completed workers before it moves output buffers into Raylib audio objects or GPU textures. Mode changes and file unloads request cancellation before they release worker-owned state.

### Reconstruction cache

The session cache stores six recent full-file reconstructions. Its total audio-data limit equals 256 MB.

Each cache key contains:

- Reconstruction algorithm.
- Selected component count.
- Output channel count.

The channel count prevents a mono buffer from satisfying a stereo request. Cache insertion and retrieval transfer buffer ownership instead of copying the audio.

## Application workspaces

Spectra contains six workspaces:

1. **Overview** describes the generated-audio and imported-audio paths.
2. **Synthesizer** edits harmonic amplitudes and generated-tone parameters.
3. **Audio Analysis** imports audio and analyzes a selected region.
4. **Harmonic Lab** compares harmonic and Fourier-frame reconstructions.
5. **Spectrogram** controls Mono FFT, Stereo FFT, and STFT reconstruction.
6. **Settings** controls text scale and the fixed-model memory limit.

The shell keeps a fixed navigation rail and a full-width status bar. F11 toggles borderless full screen. F1 opens the Help Center.

Number keys 1 through 5 select the five primary workspaces. The Settings button remains in the navigation rail.

The default window measures 1,600 by 900 pixels. The minimum window measures 1,100 by 800 pixels.

Text scale starts at 110 percent. Settings supports values from 90 through 140 percent.

Spectra loads Segoe UI on Windows when the font exists. A Chinese fallback font loads only after a filename requires non-ASCII glyphs.

## Source organization

| Path | Responsibility |
| --- | --- |
| `src/main.c` | Application state, worker orchestration, playback routing, exports, and page dispatch |
| `src/audio/audio_import.c` | Decoding, Unicode paths, channel preservation, and mono reference generation |
| `src/audio/audio_engine.c` | Raylib audio objects, transport state, and PCM WAV export |
| `src/audio/reconstruction_cache.c` | Bounded ownership-transferring reconstruction cache |
| `src/dsp/additive_synth.c` | Harmonic synthesis and ADSR rendering |
| `src/dsp/fft.c` | Dependency-free radix-2 complex FFT |
| `src/dsp/pitch_detection.c` | YIN-style pitch estimation and note conversion |
| `src/dsp/harmonic_analysis.c` | Integer-harmonic peak matching |
| `src/dsp/harmonic_resynthesis.c` | Stationary phase-free harmonic synthesis |
| `src/dsp/fourier_reconstruction.c` | Centered Fourier-frame analysis and inverse reconstruction |
| `src/dsp/global_fourier_reconstruction.c` | Incremental fixed whole-file FFT analysis and inverse reconstruction |
| `src/dsp/stft_reconstruction.c` | Spectrogram generation and STFT inverse reconstruction |
| `src/dsp/channel_mix.c` | Mono downmix and multichannel interleaving |
| `src/platform/background_task.c` | Cancellable worker abstraction |
| `src/platform/file_io.c` | UTF-8-aware file operations |
| `src/platform/windows_app.c` | Native dialogs, DPI setup, icon handling, and Windows process setup |
| `src/ui/app_shell.c` | Workspace rendering and action collection |
| `src/ui/spectrogram_layout.c` | Testable Spectrogram geometry |
| `src/ui/theme.c` | Font loading, glyph coverage, color values, and text scale |
| `src/ui/help_center.c` | Workspace tutorials |

## Native Windows build

The Windows build requires:

- CMake 3.20 or newer.
- Visual Studio 2022 C++ Build Tools.
- vcpkg.
- `raylib:x64-windows`.
- `glfw3:x64-windows`.

Configure the project:

```powershell
cmake -S . -B build-windows `
  -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake
```

Build the Release configuration:

```powershell
cmake --build build-windows --config Release
```

Run the native executable:

```powershell
.\build-windows\Release\spectra_desktop.exe
```

The vcpkg Raylib build uses custom frame control on Windows. Spectra swaps the buffer, polls Windows events, and applies 60 Hz frame pacing.

If PowerShell exposes both `PATH` and `Path`, remove the uppercase process variable before the build:

```powershell
[Environment]::SetEnvironmentVariable('PATH', $null, 'Process')
& 'C:\Program Files\CMake\bin\cmake.exe' --build build-windows --config Release
```

## Tests

CMake builds four test executables:

| Test | Coverage |
| --- | --- |
| `dsp_tests` | Synthesis, FFT, pitch, harmonic analysis, fixed FFT, STFT, channels, energy, and memory policy |
| `audio_import_tests` | Unicode paths, decoding, stereo preservation, and stereo WAV export |
| `ui_layout_tests` | Spectrogram geometry at minimum, desktop, full-screen, and ultrawide sizes |
| `runtime_tests` | Worker completion, cancellation, cache ownership, eviction, and channel-aware keys |

Build and run every test:

```powershell
cmake --build build-windows --config Release
ctest --test-dir build-windows -C Release --output-on-failure
```

Run the DSP suite directly:

```powershell
.\build-windows\Release\dsp_tests.exe
```

## Windows package

CPack creates a portable ZIP archive:

```powershell
cmake --build build-windows --config Release --target package
```

The command writes `build-windows\Spectra-1.0.0-windows-x64.zip`.

The archive contains:

- `spectra_desktop.exe`
- Raylib and GLFW runtime libraries
- Windows version metadata and manifest
- Application icon
- License
- README
- Logo assets

## Linux build

The Makefile builds the same `src/main.c` entry point when Raylib exists on the host system:

```bash
make
make test
make run
```

The Makefile writes the application to `build/spectra_desktop`.

## Current constraints

- Imported-file decoding runs on the UI thread.
- Selected-region analysis runs on the UI thread.
- Full-file reconstruction accepts at most 600 seconds of audio.
- Fixed whole-file FFT models allocate memory in proportion to the padded transform size.
- The 1.5 GB setting permits a large model allocation during the current session.
- Settings do not persist after process exit.
- The cache does not persist after process exit.
- The pitch estimator requires a stable periodic signal.
- Harmonic resynthesis discards phase and time-varying articulation.
- Fixed FFT bins do not represent detected musical harmonics.
- STFT component counts apply separately to each frame and channel.
- Retained spectral energy does not measure perceptual similarity.
- Spectrogram analysis uses the mono analysis reference.
- Sources with more than two channels produce mono output.
- The FFT favors readable implementation code over library-level optimization.
- Spectra does not identify instruments and does not use machine learning.

## License

Spectra uses the license in `LICENSE`.
