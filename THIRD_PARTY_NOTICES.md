# Third-party notices

Spectra is distributed under the MIT License. See `LICENSE` for the project license.

The portable Windows package includes dynamically linked libraries and runtime files from the projects below. Their licenses apply to those components, not to Spectra as a whole. The package stores the license text supplied by each vcpkg port in `licenses/third-party/`.

| Component | Purpose | License information |
| --- | --- | --- |
| Qt 6 (`qtbase`, `qtdeclarative`, `qtshadertools`, `qtsvg`) | Interface, QML runtime, and graphics | See the installed notices and [Qt licensing](https://www.qt.io/licensing/) |
| Raylib | Audio decoding and playback | zlib/libpng license |
| GLFW | Window and input support used by Raylib | zlib/libpng license |
| zlib | Compression | zlib license |
| libpng | PNG support | libpng license |
| PCRE2 | Regular-expression support used by Qt | BSD-3-Clause |
| double-conversion | Numeric conversion used by Qt | BSD-3-Clause |
| md4c | Markdown parsing used by Qt | MIT |
| Raylib support libraries (`cgltf`, `dirent`, `drlibs`, `miniaudio`, `mmx`, `nanosvg`, `qoi`, `stb`) | File formats, audio, and platform support | See each installed notice file |

Qt is dynamically linked in the Windows package. Corresponding Qt source releases are available from the [Qt source archive](https://download.qt.io/archive/qt/) and the [Qt code repository](https://code.qt.io/).

This file is a guide to the packaged dependencies. The license files in `licenses/third-party/` are the authoritative notices shipped with the binaries.
