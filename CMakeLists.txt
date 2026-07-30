cmake_minimum_required(VERSION 3.20)

project(SpectraDesktop LANGUAGES C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

file(GLOB_RECURSE SPECTRA_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)

add_executable(spectra_raylib ${SPECTRA_SOURCES})
target_include_directories(spectra_raylib PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")

if(WIN32)
    set_target_properties(spectra_raylib PROPERTIES WIN32_EXECUTABLE TRUE)
    if(MSVC)
        target_link_options(
            spectra_raylib
            PRIVATE
                /ENTRY:mainCRTStartup
                /MANIFEST:NO
        )
    endif()

    string(REPLACE "\\" "/" SPECTRA_ICON_PATH "${CMAKE_CURRENT_SOURCE_DIR}/assets/spectra.ico")
    string(REPLACE "\\" "/" SPECTRA_MANIFEST_PATH "${CMAKE_CURRENT_SOURCE_DIR}/src/windows/spectra.manifest")
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/src/windows/spectra.rc.in"
        "${CMAKE_CURRENT_BINARY_DIR}/spectra.rc"
        @ONLY
    )
    target_sources(spectra_raylib PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/spectra.rc")
endif()

option(
    SPECTRA_RAYLIB_CUSTOM_FRAME_CONTROL
    "Drive buffer swapping, event polling, and frame pacing for Raylib builds with custom frame control enabled"
    ${WIN32}
)
if(SPECTRA_RAYLIB_CUSTOM_FRAME_CONTROL)
    target_compile_definitions(spectra_raylib PRIVATE SPECTRA_RAYLIB_CUSTOM_FRAME_CONTROL=1)
endif()

if(MSVC)
    target_compile_options(spectra_raylib PRIVATE /W4)
else()
    target_compile_options(spectra_raylib PRIVATE -Wall -Wextra)
endif()

find_package(raylib CONFIG QUIET)
if(raylib_FOUND)
    target_link_libraries(spectra_raylib PRIVATE raylib)
else()
    find_package(raylib REQUIRED)
    target_link_libraries(spectra_raylib PRIVATE raylib)
endif()

if(WIN32)
    find_package(glfw3 CONFIG REQUIRED)
endif()

find_package(Threads REQUIRED)
target_link_libraries(spectra_raylib PRIVATE Threads::Threads)

if(WIN32)
    target_link_libraries(spectra_raylib PRIVATE winmm shell32 comdlg32)
elseif(UNIX AND NOT APPLE)
    target_link_libraries(spectra_raylib PRIVATE m)
endif()

option(
    SPECTRA_BUILD_QT_FRONTEND
    "Build the Qt Quick/QML desktop frontend"
    ON
)

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

    if(Qt6_FOUND)
        qt_standard_project_setup(REQUIRES 6.8)

        set(SPECTRA_QT_CORE_SOURCES
            "${CMAKE_CURRENT_SOURCE_DIR}/src/audio/audio_engine.c"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/audio/audio_import.c"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/additive_synth.c"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/channel_mix.c"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/dsp_types.c"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/fft.c"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/fourier_reconstruction.c"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/global_fourier_reconstruction.c"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/harmonic_analysis.c"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/harmonic_resynthesis.c"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/pitch_detection.c"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/signal_utils.c"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/stft_reconstruction.c"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/waveform_summary.c"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/windowing.c"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/file_io.c"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/monotonic_clock.c"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/windows_app.c"
        )

        qt_add_executable(
            spectra_qt
            WIN32
            "${CMAKE_CURRENT_SOURCE_DIR}/src/qt/main.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/qt/spectra_controller.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/qt/spectra_controller.h"
            ${SPECTRA_QT_CORE_SOURCES}
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

        if(MSVC)
            target_compile_options(spectra_qt PRIVATE /W4)
            target_link_options(spectra_qt PRIVATE /MANIFEST:NO)
        else()
            target_compile_options(spectra_qt PRIVATE -Wall -Wextra)
        endif()

        if(WIN32)
            target_sources(
                spectra_qt
                PRIVATE
                    "${CMAKE_CURRENT_BINARY_DIR}/spectra.rc"
            )
            target_link_libraries(
                spectra_qt
                PRIVATE
                    winmm
                    shell32
                    comdlg32
            )
        elseif(UNIX AND NOT APPLE)
            target_link_libraries(spectra_qt PRIVATE m)
        endif()

        set(SPECTRA_QML_FILES
            qml/Main.qml
            qml/Theme.qml
            qml/components/AppButton.qml
            qml/components/MetricCard.qml
            qml/components/Panel.qml
            qml/components/StatusRow.qml
            qml/components/TransportPlayer.qml
            qml/components/WorkspaceCard.qml
            qml/pages/OverviewPage.qml
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
        install(
            DIRECTORY "$<TARGET_FILE_DIR:spectra_qt>/"
            DESTINATION "${CMAKE_INSTALL_BINDIR}"
            FILES_MATCHING
                PATTERN "*.dll"
                PATTERN "Qt6*.dll" EXCLUDE
        )
        qt_generate_deploy_qml_app_script(
            TARGET spectra_qt
            OUTPUT_SCRIPT SPECTRA_QT_DEPLOY_SCRIPT
            NO_TRANSLATIONS
        )
        install(SCRIPT "${SPECTRA_QT_DEPLOY_SCRIPT}")
        install(
            FILES
                "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
                "${CMAKE_CURRENT_SOURCE_DIR}/README.md"
            DESTINATION "."
        )
    endif()
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
    "${CMAKE_CURRENT_SOURCE_DIR}/src/audio/audio_engine.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/audio/audio_import.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/channel_mix.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/dsp_types.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/signal_utils.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/file_io.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/monotonic_clock.c"
)
target_include_directories(audio_import_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(audio_import_tests PRIVATE raylib)

add_executable(ui_layout_tests
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/ui_layout_tests.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/spectrogram_layout.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/spectrum_viewport.c"
)
target_include_directories(ui_layout_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")

add_executable(runtime_tests
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/runtime_tests.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/audio/reconstruction_cache.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/dsp_types.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/background_task.c"
)
target_include_directories(runtime_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(runtime_tests PRIVATE Threads::Threads)

if(WIN32)
    target_link_libraries(dsp_tests PRIVATE winmm)
    target_link_libraries(audio_import_tests PRIVATE winmm)
elseif(UNIX AND NOT APPLE)
    target_link_libraries(dsp_tests PRIVATE m)
    target_link_libraries(audio_import_tests PRIVATE m)
    target_link_libraries(ui_layout_tests PRIVATE m)
endif()

enable_testing()
add_test(NAME dsp_tests COMMAND dsp_tests)
add_test(NAME audio_import_tests COMMAND audio_import_tests)
add_test(NAME ui_layout_tests COMMAND ui_layout_tests)
add_test(NAME runtime_tests COMMAND runtime_tests)
