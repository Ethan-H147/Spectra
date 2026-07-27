# Spectra

Spectra is a desktop Fourier audio engine for harmonic synthesis, spectral visualization, and educational DSP exploration.

Spectra explores how musical timbre emerges from harmonic structure. It lets users generate a tone from harmonic sliders, visualize the resulting waveform and frequency spectrum, and hear the result immediately in a local desktop window.

## Current Status

This version is a C + Raylib desktop app implementing Milestones 1 through 7:

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
- Sample-based fundamental pitch estimation using YIN-style periodicity detection
- Musical note, cents offset, and confidence readout for generated tones
- WAV, MP3, OGG, and FLAC import through a native picker or drag-and-drop
- Mono/stereo source preservation with a separate mono reference for analysis
- Explicit downmixing for sources with more than two channels
- Full-file and selected-region playback
- Interactive region selection with waveform, FFT, peaks, and pitch analysis
- Harmonic peak matching around integer multiples of the estimated fundamental
- Relative-amplitude harmonic tables with explicit missing-partial rows
- Additive resynthesis of stable pitched regions with original/model A/B playback
- Phase-preserving reconstruction of a selected Hann-windowed frame from its strongest Fourier bins
- Adjustable Top-N component playback and WAV export for both reconstruction models
- Whole-file Fourier analysis with one frequency ranking for the complete imported recording
- Fixed whole-file reconstructions across the complete one-sided, zero-padded FFT grid
- Separate padded-bin and retained-energy budgets with presets and exact numeric controls
- Time-varying STFT reconstructions with independently selected bins in every frame
- Background whole-file FFT and STFT workers with safe cancellation and main-thread Raylib handoff
- Reusable whole-file Fourier models plus a bounded 256 MB recent-reconstruction cache
- Stereo-aware fixed and time-varying reconstructions with per-channel component budgets
- 2048-sample Hann STFT spectrogram shared as a reference visualization for both modes
- Full-timeline spectrogram rendering with frequency and time axes
- Original/reconstruction A/B playback with pause, elapsed time, progress, and Save As WAV export
- Professional multi-workspace desktop shell with persistent navigation
- Overview page mapping the complete generated/uploaded audio pipeline
- Working Spectrogram workspace with separate fixed and time-varying reconstruction modes
- Settings page covering application, audio, analysis, and appearance defaults
- Honest per-channel component and retained-energy labels
- Native Windows per-monitor DPI behavior and application icon
- DSP, audio import/export, background-runtime, cache, and layout regression tests

Milestone 7 is the current final desktop roadmap milestone. It completes the
current architecture and quality roadmap without preventing future features
or later milestones.

## How It Works

Generated sound pipeline:

```text
harmonic settings
-> additive synthesis
-> ADSR envelope
-> normalization
-> Raylib sound buffer
-> playback
-> waveform, spectrum, peaks, and sample-derived pitch
```

Imported sound pipeline:

```text
audio file
-> decode
-> preserve mono/stereo source audio
-> create a mono analysis reference
-> selected region
-> FFT, peaks, and pitch
-> integer-harmonic extraction
-> additive resynthesis
-> original/model A/B playback
```

Spectra also ranks the complex Fourier coefficients of the centered analysis frame. Unlike the phase-free harmonic model, the Top-N frame reconstruction retains each selected bin's phase and uses an inverse FFT to rebuild the windowed frame.

For complete-file reconstruction, Spectra can build either one reusable
zero-padded Fourier model from the mono analysis reference or one model per
preserved source channel. The mono model is the safe default when a long
stereo transform would exceed the configured memory limit:

```text
mono reference or preserved mono/stereo audio
-> zero-pad to a power-of-two length
-> one mono whole-file FFT or one FFT per source channel
-> rank one fixed set of complex frequency coefficients in each model
-> keep the strongest N coefficients and their conjugate mirror pairs
-> inverse FFT
-> trim to the original duration
-> explicitly labeled mono/stereo padded-FFT-bin playback and WAV export
```

Top 5 therefore means five fixed sinusoidal frequencies for the entire file,
not five new bins in every time frame. A speech recording cannot preserve its
phoneme timing or natural pitch motion at that setting. The STFT spectrogram is
computed separately for visualization and does not choose reconstruction
components.

The alternative **Time-varying STFT** mode restores the earlier behavior:

```text
mono or stereo audio
-> overlapping 2048-sample Hann frames
-> rank frequency bins independently in each frame and channel
-> keep the strongest N bins and their phases
-> inverse FFT and normalized overlap-add
-> recognizable full-length playback and WAV export
```

This mode can preserve changing speech and music with far fewer components
because its selected frequencies are allowed to change every 512 samples. In
fixed whole-file mode, the bin-count controls explicitly refer to the
one-sided bins of the power-of-two, zero-padded FFT. Zero-padding makes the
frequency grid denser for efficient processing; it does not add source
information. The separate energy mode chooses the smallest strongest-bin set
that reaches an exact retained-spectral-energy target in every channel.
Time-varying STFT mode accepts 1-1,023 components per frame per channel.
All whole-file DSP runs on background workers. The reusable Fourier models
remain in memory for the imported file, while recently rendered component
counts can be restored from a bounded cache without recomputation.

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
    audio_import.c
    audio_import.h
    reconstruction_cache.c
    reconstruction_cache.h
  dsp/
    additive_synth.c
    additive_synth.h
    channel_mix.c
    channel_mix.h
    dsp_types.c
    dsp_types.h
    fft.c
    fft.h
    fourier_reconstruction.c
    fourier_reconstruction.h
    global_fourier_reconstruction.c
    global_fourier_reconstruction.h
    harmonic_analysis.c
    harmonic_analysis.h
    harmonic_resynthesis.c
    harmonic_resynthesis.h
    pitch_detection.c
    pitch_detection.h
    signal_utils.c
    signal_utils.h
    stft_reconstruction.c
    stft_reconstruction.h
    windowing.c
    windowing.h
    waveform_summary.c
    waveform_summary.h
  ui/
    app_shell.c
    app_shell.h
    theme.c
    theme.h
    widgets.c
    widgets.h
    spectrogram_layout.c
    spectrogram_layout.h
  platform/
    background_task.c
    background_task.h
    file_io.c
    file_io.h
    windows_app.c
    windows_app.h
tests/
  audio_import_tests.c
  dsp_tests.c
  runtime_tests.c
  ui_layout_tests.c
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
cmake --build build-windows --config Release
ctest --test-dir build-windows -C Release --output-on-failure
```

The tests check sine generation length, Hann window values, additive synthesis normalization, spectral peak detection, FFT output, clean-tone pitch accuracy, missing-fundamental recovery, musical-note mapping, silence rejection, channel mixing/interleaving, waveform summarization, harmonic matching, resynthesis safety, Fourier ranking, strided stereo FFT/STFT reconstruction, visualization-only STFT output, spectrogram dimensions, full-bin inverse reconstruction, Unicode-path import, stereo WAV round trips, worker completion/cancellation, bounded cache ownership/eviction, and responsive Spectrogram layouts without overlap.

## Controls

- Use the left workspace rail or number keys **1–5** to switch between Overview, Synthesizer, Audio Analysis, Harmonic Lab, and Spectrogram.
- Open **Settings** with the gear button at the lower-left corner.
- Adjust interface typography from **90%–140%** with the A− / A+ controls under Settings → Appearance. The redesigned 100% baseline is readable, Spectra starts at 110%, and clicking the percentage resets it.
- Press **Play tone** or the spacebar to hear the current tone.
- In **Audio Analysis**, choose or drop a WAV, MP3, OGG, or FLAC file, adjust the selected region, and press **Analyze selected region**.
- Use **Play all**, **Play region**, and **Stop** to compare the source with the region being analyzed.
- Open **Harmonic Lab** after analysis to compare the original region with its phase-free harmonic model.
- Adjust **Strongest frequency components** to rebuild the centered windowed frame with a different Top-N Fourier budget.
- Use **Play frame** and **Play Top-N** for a phase-preserving frame comparison.
- In **Spectrogram**, use the fixed **Mono FFT** or **Stereo FFT** models for the sparse sine-wave build-up effect, or **STFT** for a more recognizable time-varying reconstruction.
- Choose **Mono FFT** for the memory-safe fixed model, **Stereo FFT** to preserve separate left/right models, or **STFT** for a changing per-frame model. Long stereo files automatically start in Mono FFT.
- Under **FFT bins**, choose a padded-bin preset or type an exact count. The count is per channel only in Stereo FFT.
- Under **Energy**, choose or type a retained-spectral-energy target; Spectra resolves a shared per-channel count that reaches the target in every channel.
- Under **Settings**, click **Whole-file FFT memory** to cycle between 512 MB, 768 MB, 1 GB, and 1.5 GB. Raising it can unlock Stereo FFT for long files and intentionally allows higher RAM use.
- Use **Play original** and **Play Top-N** to compare the complete recording. The shared player provides pause, elapsed time, progress, and stop controls; export uses a mode-specific WAV filename.
- Resize or maximize the window; the workspace reflows to the live window size while the sidebar stays left and the footer spans the bottom edge.
- Press **F11** to enter or leave borderless full screen.
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
- YIN-style pitch estimation
- Cumulative mean normalized difference
- Harmonic peak matching in cents
- Complex Fourier phase
- Inverse FFT reconstruction
- Short-time Fourier transform (STFT)
- Fixed-component whole-file inverse FFT
- Time-varying inverse STFT and normalized overlap-add
- Progressive spectral reconstruction

## Limitations

Spectra is an educational DSP project, not a commercial pitch detector or production audio engine.

Current limitations:

- Imported audio decoding and selected-region analysis begin synchronously; very large files may take a moment to load.
- Whole-file FFT and STFT work runs on background workers, but imports remain limited to ten minutes for predictable memory use.
- The fixed whole-file model starts with a 768 MB safety budget. When a long stereo transform would exceed it, Spectra automatically uses the one-model Mono FFT path; the user can explicitly select Stereo FFT and raise the session limit up to 1.5 GB in Settings.
- The complete fixed-mode bin range comes from a zero-padded power-of-two transform. It is a computational grid, not a count of independent audible frequencies or source information.
- Long or high-sample-rate files can still require substantial memory and processing time even though the UI remains responsive.
- Mono and stereo files preserve their output layout. Sources with more than two channels are explicitly downmixed to mono because surround-channel mapping is outside the current roadmap.
- The pitch, waveform, and spectrogram views use a mono reference even when reconstruction and full-file playback remain stereo.
- The reconstruction cache is session-local and bounded to 256 MB; an evicted setting is recomputed from the reusable model.
- Imported-audio pitch estimates are most useful on stable, pitched regions.
- Harmonic resynthesis is intentionally phase-free and stationary, so it approximates pitched timbre rather than preserving transients, noise, or changing articulation.
- Whole-file Top-N entries are fixed FFT frequencies, not necessarily integer musical harmonics of a detected fundamental.
- Time-varying Top-N entries are selected independently per frame and channel and should not be interpreted as one fixed set of physical harmonics.
- The FFT implementation is intentionally readable and dependency-free, not highly optimized.
- Spectra does not use AI or machine learning.
- Spectra does not perfectly identify instruments or timbre.

## Roadmap

- Milestone 3 (complete): estimate pitch from generated sample buffers and display frequency, note, cents, and confidence.
- Milestone 4 (complete): load WAV, MP3, OGG, and FLAC audio, provide playback/region selection, and convert analysis data to mono.
- Milestone 5A (complete): extract true harmonics from pitched sounds and resynthesize an additive approximation.
- Milestone 5B (complete): reconstruct a selected frame from its strongest phase-preserving Fourier components.
- Milestone 6 (complete): add separate fixed whole-file and time-varying STFT Top-N reconstruction modes, spectrogram rendering, arbitrary component counts, A/B playback, and WAV export.
- Milestone 7 (complete): preserve mono/stereo reconstruction, reuse whole-file models and recent results, move full-file DSP to cancellable background workers, improve Windows DPI behavior, and expand UI, help, and regression coverage.

Milestone 7 is the final milestone in the current desktop roadmap. Future
ideas can be tracked as new milestones instead of silently expanding this
roadmap.
