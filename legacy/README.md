# Legacy frontends

This directory preserves interfaces that are no longer the canonical Spectra application.

## `raylib`

`raylib` contains the complete pre-Qt desktop frontend. It still compiles against the shared audio, DSP, and platform code under `src`.

The frontend is preserved for reference and comparison. New interface work belongs in `qml` and `src/qt`. Changes to shared DSP or audio code should continue to compile with the legacy target when practical.

Build it with CMake:

```powershell
cmake -S . -B build-raylib `
  -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake `
  -DVCPKG_TARGET_TRIPLET=x64-windows `
  -DVCPKG_MANIFEST_NO_DEFAULT_FEATURES=ON `
  -DSPECTRA_BUILD_QT_FRONTEND=OFF `
  -DSPECTRA_BUILD_RAYLIB_LEGACY=ON

cmake --build build-raylib --config Release --target spectra_raylib
```

On Linux, run the retained Makefile from this directory:

```bash
cd legacy/raylib
make
make test
make run
```

The `raylib-v1` tag records the last Raylib-first repository state.

## `prototype`

`prototype` contains the initial WAV-header and waveform-viewer exercise. It is not part of the current build.
