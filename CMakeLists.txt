cmake_minimum_required(VERSION 3.20)

project(SpectraDesktop C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

file(GLOB_RECURSE SPECTRA_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)

add_executable(spectra_desktop ${SPECTRA_SOURCES})
target_include_directories(spectra_desktop PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")

if(WIN32)
    set_target_properties(spectra_desktop PROPERTIES WIN32_EXECUTABLE TRUE)
    if(MSVC)
        target_link_options(spectra_desktop PRIVATE /ENTRY:mainCRTStartup)
    endif()

    string(REPLACE "\\" "/" SPECTRA_ICON_PATH "${CMAKE_CURRENT_SOURCE_DIR}/assets/spectra.ico")
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/src/windows/spectra.rc.in"
        "${CMAKE_CURRENT_BINARY_DIR}/spectra.rc"
        @ONLY
    )
    target_sources(spectra_desktop PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/spectra.rc")
endif()

option(
    SPECTRA_RAYLIB_CUSTOM_FRAME_CONTROL
    "Drive buffer swapping, event polling, and frame pacing for Raylib builds with custom frame control enabled"
    ${WIN32}
)
if(SPECTRA_RAYLIB_CUSTOM_FRAME_CONTROL)
    target_compile_definitions(spectra_desktop PRIVATE SPECTRA_RAYLIB_CUSTOM_FRAME_CONTROL=1)
endif()

if(MSVC)
    target_compile_options(spectra_desktop PRIVATE /W4)
else()
    target_compile_options(spectra_desktop PRIVATE -Wall -Wextra)
endif()

find_package(raylib CONFIG QUIET)
if(raylib_FOUND)
    target_link_libraries(spectra_desktop PRIVATE raylib)
else()
    find_package(raylib REQUIRED)
    target_link_libraries(spectra_desktop PRIVATE raylib)
endif()

if(WIN32)
    target_link_libraries(spectra_desktop PRIVATE winmm shell32 comdlg32)
elseif(UNIX AND NOT APPLE)
    target_link_libraries(spectra_desktop PRIVATE m)
endif()

file(GLOB_RECURSE SPECTRA_DSP_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/*.c"
)

add_executable(dsp_tests
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/dsp_tests.c"
    ${SPECTRA_DSP_SOURCES}
)
target_include_directories(dsp_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")

add_executable(audio_import_tests
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/audio_import_tests.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/audio/audio_import.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/channel_mix.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/dsp_types.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/signal_utils.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/file_io.c"
)
target_include_directories(audio_import_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(audio_import_tests PRIVATE raylib)

if(WIN32)
    target_link_libraries(dsp_tests PRIVATE winmm)
    target_link_libraries(audio_import_tests PRIVATE winmm)
elseif(UNIX AND NOT APPLE)
    target_link_libraries(dsp_tests PRIVATE m)
    target_link_libraries(audio_import_tests PRIVATE m)
endif()

enable_testing()
add_test(NAME dsp_tests COMMAND dsp_tests)
add_test(NAME audio_import_tests COMMAND audio_import_tests)
