# Spectra

Spectra is a desktop application for inspecting Fourier representations of audio. The canonical interface uses Qt Quick and QML. The signal-processing core uses C99.

Raylib remains a runtime dependency for audio decoding and playback. It no longer controls the canonical application layout.

## Repository status

`spectra_qt` is the primary application and the default frontend.

The `raylib-v1` tag preserves the retired Raylib-first application. The current source tree contains only the Qt frontend.

Generated build trees, compiled binaries, editor state, and local audio files are not stored in the repository.

## Functions

Spectra provides six workspaces:

1. **Overview** reports source, analysis, reconstruction, and audio-device state.
2. **Synthesizer** generates tones from 16 adjustable harmonics.
3. **Audio Analysis** displays a waveform, spectrum, peaks, and pitch estimate.
4. **Harmonic Lab** compares harmonic resynthesis with Fourier-frame reconstruction.
5. **Spectrogram** displays STFT magnitude and builds complete-file reconstructions.
6. **Settings** controls text scale and the fixed-model memory limit.

Imported audio supports WAV, MP3, OGG, and FLAC. Mono and stereo sources retain their playback channel layout. Sources with more than two channels produce mono playback and analysis buffers.

## Reconstruction models

| Model | Analysis scope | Selection unit | Phase | Output |
| --- | --- | --- | --- | --- |
| Additive synthesizer | Generated tone | 16 integer harmonics | Generated oscillator phase | Mono |
| Harmonic resynthesis | Selected pitched region | Detected integer harmonics | Discarded | Mono |
| Fourier frame | One centered Hann frame | Ranked complex Fourier components | Preserved | Mono frame |
| Mono FFT | Complete audio file | Ranked one-sided FFT bins | Preserved | Mono |
| Stereo FFT | Complete left and right channels | Ranked FFT bins per channel | Preserved | Stereo |
| STFT | Overlapping frames | FFT bins per frame and channel | Preserved | Source channel layout |

These component counts describe different models. A five-bin whole-file FFT uses five frequencies for the complete file. A five-bin STFT selects five frequencies in every frame and channel.

The fixed whole-file FFT supports two selection methods:

- **FFT bins** retains an exact number of ranked bins.
- **Spectral energy** finds the smallest ranked-bin set that reaches an energy target.

The STFT path uses a 2,048-sample Hann window and a 512-sample hop. Normalized overlap-add combines inverse-transformed frames.

## Source organization

| Path | Responsibility |
| --- | --- |
| `src/qt/` | Qt startup and the QML-facing application controller |
| `qml/` | Canonical application shell, controls, and workspace layouts |
| `src/audio/` | Import, playback, export, and reconstruction cache |
| `src/dsp/` | Synthesis, FFT, pitch, peak, harmonic, and STFT processing |
| `src/platform/` | Background tasks, file access, timing, and Windows integration |
| `tests/` | Canonical audio, DSP, and runtime tests |

Full-file spectrogram generation, FFT analysis, inverse FFT reconstruction, and STFT reconstruction run on background workers. The UI thread polls progress and transfers completed buffers into playback or display objects.

## Windows build

Requirements:

- CMake 3.20 or newer
- Visual Studio 2022 C++ Build Tools
- vcpkg
- Qt 6.8 or newer with Qt Quick and QML
- `raylib:x64-windows`

Configure:

```powershell
cmake -S . -B build `
  -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake `
  -DVCPKG_TARGET_TRIPLET=x64-windows
```

Build the canonical application:

```powershell
cmake --build build --config Release --target spectra_qt
```

Run:

```powershell
.\build\Release\spectra_qt.exe
```

CMake deploys the required Qt, Raylib, and GLFW runtime files beside the Release executable.

If PowerShell exposes both `PATH` and `Path`, normalize the process environment before invoking MSBuild:

```powershell
$savedPath = $env:Path
Remove-Item Env:Path -ErrorAction SilentlyContinue
$env:Path = $savedPath
cmake --build build --config Release --target spectra_qt
```

## Package

Install the executable and runtime files into a separate directory:

```powershell
cmake --install build `
  --config Release `
  --prefix build\package
```

The packaged executable is `build\package\bin\spectra_qt.exe`.

## Tests

The default configuration registers three test executables:

| Test | Coverage |
| --- | --- |
| `dsp_tests` | Synthesis, FFT, pitch, harmonic analysis, fixed FFT, STFT, channels, energy, and memory policy |
| `audio_import_tests` | Unicode paths, decoding, channel preservation, and WAV export |
| `runtime_tests` | Worker completion, cancellation, cache ownership, eviction, and channel-aware keys |

Build and run the test suite:

```powershell
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

## Constraints

- Imported-file decoding and selected-region analysis run on the UI thread.
- Full-file reconstruction accepts at most 600 seconds of audio.
- Fixed FFT memory grows with the zero-padded transform size and retained-bin capacity.
- Spectral-energy selection ranks the complete one-sided FFT.
- Settings and reconstruction caches do not persist after process exit.
- Pitch estimation requires stable periodic content.
- Harmonic resynthesis discards phase and time-varying articulation.
- STFT component counts apply separately to each frame and channel.
- Spectra does not identify instruments or use machine learning.

## License

Spectra uses the license in [`LICENSE`](LICENSE).
