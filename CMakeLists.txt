cmake_minimum_required(VERSION 3.20)

project(SpectraDesktop VERSION 1.0.1 LANGUAGES C CXX)

set(
    SPECTRA_BUILD_COMMIT
    "local"
    CACHE STRING
    "Source commit recorded in the application diagnostics"
)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(
    SPECTRA_BUILD_QT_FRONTEND
    "Build the canonical Qt Quick/QML desktop application"
    ON
)

find_package(raylib CONFIG QUIET)
if(NOT raylib_FOUND)
    find_package(raylib REQUIRED)
endif()

find_package(Threads REQUIRED)

if(WIN32)
    string(
        REPLACE "\\" "/"
        SPECTRA_ICON_PATH
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/spectra.ico"
    )
    string(
        REPLACE "\\" "/"
        SPECTRA_MANIFEST_PATH
        "${CMAKE_CURRENT_SOURCE_DIR}/src/windows/spectra.manifest"
    )
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/src/windows/spectra.rc.in"
        "${CMAKE_CURRENT_BINARY_DIR}/spectra.rc"
        @ONLY
    )
endif()

set(SPECTRA_CORE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/audio/audio_engine.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/audio/audio_import.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/audio/reconstruction_cache.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/additive_synth.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/channel_mix.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/dsp_types.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/fft.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/fft_plan.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/fourier_reconstruction.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/global_fourier_reconstruction.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/harmonic_analysis.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/harmonic_resynthesis.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/pitch_detection.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/signal_utils.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/spectral_effects.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/stft_reconstruction.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/waveform_summary.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/windowing.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/background_task.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/file_io.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/monotonic_clock.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/parallel_for.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/windows_app.c"
)

function(spectra_enable_warnings target_name)
    if(MSVC)
        target_compile_options(${target_name} PRIVATE /W4)
    else()
        target_compile_options(${target_name} PRIVATE -Wall -Wextra)
    endif()
endfunction()

function(spectra_link_platform target_name)
    if(UNIX AND NOT APPLE)
        target_link_libraries(${target_name} PRIVATE m)
    endif()
endfunction()

if(SPECTRA_BUILD_QT_FRONTEND)
    find_package(
        Qt6 6.8
        COMPONENTS
            Core
            Gui
            Qml
            Quick
            QuickControls2
        REQUIRED
    )

    qt_standard_project_setup(REQUIRES 6.8)

    qt_add_executable(
        spectra_qt
        WIN32
        "${CMAKE_CURRENT_SOURCE_DIR}/src/qt/main.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/qt/spectra_controller.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/qt/spectra_controller.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/qt/spectrogram_pdf_export.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/qt/spectrogram_pdf_export.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/qt/spectrum_item.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/qt/spectrum_item.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/qt/spectrum_lod.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/qt/spectrum_lod.h"
        ${SPECTRA_CORE_SOURCES}
    )

    target_include_directories(
        spectra_qt
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/src"
    )
    target_link_libraries(
        spectra_qt
        PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Qml
            Qt6::Quick
            Qt6::QuickControls2
            raylib
            Threads::Threads
    )
    target_compile_definitions(
        spectra_qt
        PRIVATE
            SPECTRA_VERSION="${PROJECT_VERSION}"
            SPECTRA_BUILD_COMMIT="${SPECTRA_BUILD_COMMIT}"
    )
    spectra_enable_warnings(spectra_qt)
    spectra_link_platform(spectra_qt)

    if(MSVC)
        target_link_options(spectra_qt PRIVATE /MANIFEST:NO)
    endif()
    if(WIN32)
        target_sources(
            spectra_qt
            PRIVATE
                "${CMAKE_CURRENT_BINARY_DIR}/spectra.rc"
        )
        target_link_libraries(spectra_qt PRIVATE shell32)
    endif()

    set(SPECTRA_QML_FILES
        qml/Main.qml
        qml/Theme.qml
        qml/components/AppButton.qml
        qml/components/MetricCard.qml
        qml/components/Panel.qml
        qml/components/SegmentedSwitch.qml
        qml/components/SpectraIcon.qml
        qml/components/SpectrumMovementView.qml
        qml/components/RangeEqSpectrumView.qml
        qml/components/StatusRow.qml
        qml/components/TransportPlayer.qml
        qml/components/WorkspaceCard.qml
        qml/pages/OverviewPage.qml
        qml/pages/PitchShiftPage.qml
        qml/pages/RangeEqPage.qml
        qml/pages/AnalysisPage.qml
        qml/pages/HarmonicLabPage.qml
        qml/pages/SpectrogramPage.qml
        qml/pages/SettingsPage.qml
        qml/pages/SynthPage.qml
    )
    foreach(SPECTRA_QML_FILE IN LISTS SPECTRA_QML_FILES)
        get_filename_component(
            SPECTRA_QML_RESOURCE_NAME
            "${SPECTRA_QML_FILE}"
            NAME
        )
        set_source_files_properties(
            "${SPECTRA_QML_FILE}"
            PROPERTIES
                QT_RESOURCE_ALIAS "${SPECTRA_QML_RESOURCE_NAME}"
        )
    endforeach()

    qt_add_qml_module(
        spectra_qt
        URI Spectra
        VERSION 1.0
        QML_FILES
            ${SPECTRA_QML_FILES}
        RESOURCES
            assets/spectra-icon.png
            assets/spectra-logo.svg
    )

    if(WIN32)
        add_custom_command(
            TARGET spectra_qt
            POST_BUILD
            COMMAND
                "$<TARGET_FILE:Qt6::windeployqt>"
                --qmldir "${CMAKE_CURRENT_SOURCE_DIR}/qml"
                --dir "."
                --qml-deploy-dir "Qt6/qml"
                --no-translations
                "$<TARGET_FILE_NAME:spectra_qt>"
            COMMAND
                "${CMAKE_COMMAND}"
                -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/src/qt/qt.conf"
                "$<TARGET_FILE_DIR:spectra_qt>/qt.conf"
            WORKING_DIRECTORY "$<TARGET_FILE_DIR:spectra_qt>"
            COMMENT "Deploying the Qt runtime beside spectra_qt"
            VERBATIM
        )
    endif()

    include(GNUInstallDirs)
    install(
        TARGETS spectra_qt
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    )
    if(WIN32)
        set(
            SPECTRA_NON_QT_RUNTIME_DLLS
            double-conversion.dll
            dxcompiler.dll
            dxil.dll
            glfw3.dll
            libpng16.dll
            md4c.dll
            pcre2-16.dll
            raylib.dll
            z.dll
        )
        foreach(SPECTRA_RUNTIME_DLL IN LISTS SPECTRA_NON_QT_RUNTIME_DLLS)
            install(
                FILES "$<TARGET_FILE_DIR:spectra_qt>/${SPECTRA_RUNTIME_DLL}"
                DESTINATION "${CMAKE_INSTALL_BINDIR}"
            )
        endforeach()
    endif()
    qt_generate_deploy_qml_app_script(
        TARGET spectra_qt
        OUTPUT_SCRIPT SPECTRA_QT_DEPLOY_SCRIPT
        NO_TRANSLATIONS
    )
    install(SCRIPT "${SPECTRA_QT_DEPLOY_SCRIPT}")
    install(
        FILES
            "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
            "${CMAKE_CURRENT_SOURCE_DIR}/PRIVACY.md"
            "${CMAKE_CURRENT_SOURCE_DIR}/SECURITY.md"
            "${CMAKE_CURRENT_SOURCE_DIR}/CODE_SIGNING_POLICY.md"
            "${CMAKE_CURRENT_SOURCE_DIR}/README.md"
            "${CMAKE_CURRENT_SOURCE_DIR}/THIRD_PARTY_NOTICES.md"
        DESTINATION "."
    )
    install(
        DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/docs/images/"
        DESTINATION "docs/images"
    )

    set(
        SPECTRA_RUNTIME_LICENSE_PORTS
        cgltf
        dirent
        double-conversion
        drlibs
        glfw3
        libpng
        md4c
        miniaudio
        mmx
        nanosvg
        pcre2
        qoi
        qtbase
        qtdeclarative
        qtshadertools
        qtsvg
        raylib
        stb
        zlib
    )
    if(DEFINED VCPKG_INSTALLED_DIR AND DEFINED VCPKG_TARGET_TRIPLET)
        foreach(SPECTRA_LICENSE_PORT IN LISTS SPECTRA_RUNTIME_LICENSE_PORTS)
            set(
                SPECTRA_PORT_COPYRIGHT
                "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/share/${SPECTRA_LICENSE_PORT}/copyright"
            )
            if(EXISTS "${SPECTRA_PORT_COPYRIGHT}")
                install(
                    FILES "${SPECTRA_PORT_COPYRIGHT}"
                    DESTINATION "licenses/third-party"
                    RENAME "${SPECTRA_LICENSE_PORT}.txt"
                )
            endif()
        endforeach()
    endif()
endif()

if(SPECTRA_BUILD_QT_FRONTEND)
    add_executable(
        spectrogram_pdf_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/spectrogram_pdf_tests.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/qt/spectrogram_pdf_export.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/qt/spectrogram_pdf_export.h"
    )
    target_include_directories(
        spectrogram_pdf_tests
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/src"
    )
    target_link_libraries(
        spectrogram_pdf_tests
        PRIVATE
            Qt6::Core
            Qt6::Gui
    )
    spectra_enable_warnings(spectrogram_pdf_tests)
endif()

file(
    GLOB_RECURSE
    SPECTRA_DSP_SOURCES
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/*.c"
)

add_executable(
    dsp_tests
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/dsp_tests.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/parallel_for.c"
    ${SPECTRA_DSP_SOURCES}
)
target_include_directories(
    dsp_tests
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
spectra_enable_warnings(dsp_tests)

add_executable(
    dsp_benchmark
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/dsp_benchmark.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/monotonic_clock.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/parallel_for.c"
    ${SPECTRA_DSP_SOURCES}
)
target_include_directories(
    dsp_benchmark
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
spectra_enable_warnings(dsp_benchmark)

add_executable(
    audio_import_tests
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/audio_import_tests.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/audio/audio_engine.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/audio/audio_import.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/channel_mix.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/dsp_types.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/signal_utils.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/file_io.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/monotonic_clock.c"
)
target_include_directories(
    audio_import_tests
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_link_libraries(audio_import_tests PRIVATE raylib)
spectra_enable_warnings(audio_import_tests)

add_executable(
    runtime_tests
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/runtime_tests.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/audio/reconstruction_cache.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/dsp_types.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/background_task.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/parallel_for.c"
)

add_executable(
    spectrum_lod_tests
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/spectrum_lod_tests.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/qt/spectrum_lod.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/qt/spectrum_lod.h"
)
target_include_directories(
    spectrum_lod_tests
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
spectra_enable_warnings(spectrum_lod_tests)
target_include_directories(
    runtime_tests
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_link_libraries(dsp_tests PRIVATE Threads::Threads)
target_link_libraries(dsp_benchmark PRIVATE Threads::Threads)
target_link_libraries(runtime_tests PRIVATE Threads::Threads)
spectra_enable_warnings(runtime_tests)

spectra_link_platform(dsp_tests)
spectra_link_platform(dsp_benchmark)
spectra_link_platform(audio_import_tests)

enable_testing()
add_test(NAME dsp_tests COMMAND dsp_tests)
add_test(NAME audio_import_tests COMMAND audio_import_tests)
add_test(NAME runtime_tests COMMAND runtime_tests)
add_test(NAME spectrum_lod_tests COMMAND spectrum_lod_tests)
if(TARGET spectrogram_pdf_tests)
    add_test(NAME spectrogram_pdf_tests COMMAND spectrogram_pdf_tests)
endif()
