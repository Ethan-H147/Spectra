import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    signal importRequested()

    Theme {
        id: theme
    }

    ScrollView {
        id: scrollView
        anchors.fill: parent
        clip: true
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ColumnLayout {
            width: scrollView.availableWidth
            spacing: theme.space2

            RowLayout {
                Layout.fillWidth: true
                spacing: theme.space2

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: theme.space1

                    Text {
                        text: "Current session"
                        color: theme.text
                        font.family: theme.headingFamily
                        font.pixelSize: theme.fontSize(24)
                        font.weight: Font.DemiBold
                    }

                    Text {
                        text: spectra.sourceLoaded ? spectra.sourceFileName : "No audio imported"
                        color: theme.muted
                        font.family: theme.bodyFamily
                        font.pixelSize: theme.fontSize(12)
                        elide: Text.ElideRight
                    }
                }

                AppButton {
                    text: spectra.sourceLoaded ? "Replace audio" : "Import audio"
                    onClicked: root.importRequested()
                }
            }

            GridLayout {
                id: metricGrid
                Layout.fillWidth: true
                columns: width < 960 ? 2 : 4
                columnSpacing: 12
                rowSpacing: 12

                MetricCard {
                    Layout.fillWidth: true
                    label: "AUDIO DEVICE"
                    value: spectra.audioReady ? "Ready" : "Unavailable"
                    detail: spectra.audioReady ? "System output" : "WAV export remains available"
                    stateColor: spectra.audioReady ? theme.success : theme.warning
                }

                MetricCard {
                    Layout.fillWidth: true
                    label: "SYNTH"
                    value: spectra.synthFrequency.toFixed(1) + " Hz"
                    detail: "16 partials | " + spectra.synthDuration.toFixed(2) + " s"
                    stateColor: theme.success
                }

                MetricCard {
                    Layout.fillWidth: true
                    label: "IMPORTED SOURCE"
                    value: spectra.sourceLoaded ? spectra.sourceFileName : "No source"
                    detail: spectra.sourceLoaded
                        ? (spectra.sourceSampleRate / 1000).toFixed(1) + " kHz | "
                            + (spectra.sourceChannels === 1 ? "Mono" : "Stereo")
                        : "Import WAV, MP3, OGG, or FLAC"
                    stateColor: spectra.sourceLoaded ? theme.success : theme.quiet
                }

                MetricCard {
                    Layout.fillWidth: true
                    label: "REGION ANALYSIS"
                    value: spectra.analysisReady ? spectra.analysisPitch : "Not analyzed"
                    detail: spectra.analysisReady
                        ? spectra.analysisPeakCount + " peaks | "
                            + Math.round(spectra.analysisConfidence * 100) + "% confidence"
                        : "Select a region in Audio Analysis"
                    stateColor: spectra.analysisReady ? theme.success : theme.quiet
                }
            }

            GridLayout {
                Layout.fillWidth: true
                columns: width < 900 ? 1 : 2
                columnSpacing: theme.space2
                rowSpacing: theme.space2

                Panel {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    title: "Imported source"
                    subtitle: spectra.sourceLoaded ? spectra.sourceFileName : "No imported audio"

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: theme.space1

                        Text {
                            Layout.fillWidth: true
                            text: spectra.sourceLoaded
                                ? spectra.sourceDuration.toFixed(2) + " s | "
                                    + spectra.sourceSampleRate + " Hz | "
                                    + spectra.sourceChannels + " channel"
                                    + (spectra.sourceChannels === 1 ? "" : "s")
                                : "Import audio to start an analysis session."
                            color: theme.text
                            font.family: theme.headingFamily
                            font.pixelSize: theme.fontSize(16)
                            font.weight: Font.DemiBold
                            wrapMode: Text.Wrap
                        }

                        Text {
                            Layout.fillWidth: true
                            text: spectra.sourceLoaded
                                ? "The original file is ready for playback and analysis."
                                : "Supported formats: WAV, MP3, OGG, FLAC"
                            color: theme.muted
                            font.family: theme.bodyFamily
                            font.pixelSize: theme.fontSize(12)
                            wrapMode: Text.Wrap
                        }

                        Item {
                            Layout.fillHeight: true
                            implicitHeight: theme.space2
                        }

                        TransportPlayer {
                            Layout.fillWidth: true
                            visible: spectra.sourceLoaded
                            title: "Original full file"
                            playing: spectra.sourcePlaying
                            position: spectra.playbackPosition
                            duration: spectra.sourceDuration
                            onToggleRequested: spectra.toggleSourcePlayback()
                            onStopRequested: spectra.stopPlayback()
                        }

                        AppButton {
                            visible: !spectra.sourceLoaded
                            text: "Import audio"
                            onClicked: root.importRequested()
                        }
                    }
                }

                Panel {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    title: "Analysis and reconstruction"
                    subtitle: "Select a row to open its workspace"

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: theme.space1

                        StatusRow {
                            Layout.fillWidth: true
                            label: "Region analysis"
                            value: spectra.analysisReady
                                ? spectra.analysisPitch + " | " + spectra.analysisPeakCount + " peaks"
                                : (spectra.sourceLoaded ? "Region not analyzed" : "Import audio first")
                            stateColor: spectra.analysisReady ? theme.success : theme.quiet
                            onActivated: spectra.setCurrentPage(2)
                        }

                        StatusRow {
                            Layout.fillWidth: true
                            label: "Harmonic model"
                            value: spectra.harmonicReady
                                ? spectra.detectedHarmonicCount + " / " + spectra.harmonicCount + " detected"
                                : "Waiting for pitched region"
                            stateColor: spectra.harmonicReady ? theme.success : theme.quiet
                            onActivated: spectra.setCurrentPage(3)
                        }

                        StatusRow {
                            Layout.fillWidth: true
                            label: "Fourier frame"
                            value: spectra.fourierFrameReady
                                ? spectra.fourierSelectedComponents + " components rendered"
                                : "Waiting for region analysis"
                            stateColor: spectra.fourierFrameReady ? theme.success : theme.quiet
                            onActivated: spectra.setCurrentPage(3)
                        }

                        StatusRow {
                            Layout.fillWidth: true
                            label: "Full-file model"
                            value: spectra.fullFileProcessing
                                ? Math.round(spectra.fullFileProgress * 100) + "% complete"
                                : (spectra.fullFileReady
                                    ? spectra.fullFileSelectedComponents + " components"
                                    : (spectra.sourceLoaded
                                        ? "Source ready"
                                        : "Import audio first"))
                            stateColor: spectra.fullFileProcessing
                                ? theme.accent
                                : (spectra.fullFileReady
                                    ? theme.success
                                    : (spectra.sourceLoaded ? theme.warning : theme.quiet))
                            onActivated: spectra.setCurrentPage(4)
                        }
                    }
                }
            }

            Text {
                text: "Workspaces"
                color: theme.text
                font.family: theme.headingFamily
                font.pixelSize: theme.fontSize(16)
                font.weight: Font.DemiBold
            }

            GridLayout {
                Layout.fillWidth: true
                columns: width < 960 ? 2 : 4
                columnSpacing: 12
                rowSpacing: 12

                WorkspaceCard {
                    Layout.fillWidth: true
                    Layout.minimumHeight: 196
                    title: "Synthesizer"
                    value: spectra.synthFrequency.toFixed(1) + " Hz"
                    detail: "16 partials | " + spectra.synthDuration.toFixed(2) + " s"
                    statusText: "READY"
                    stateColor: theme.success
                    onActivated: spectra.setCurrentPage(1)
                }

                WorkspaceCard {
                    Layout.fillWidth: true
                    Layout.minimumHeight: 196
                    title: "Audio Analysis"
                    value: spectra.sourceLoaded ? spectra.sourceFileName : "No source"
                    detail: spectra.sourceLoaded ? "Choose and analyze a region" : "Import a local audio file"
                    statusText: spectra.sourceLoaded ? "READY" : "EMPTY"
                    stateColor: spectra.sourceLoaded ? theme.success : theme.quiet
                    onActivated: spectra.setCurrentPage(2)
                }

                WorkspaceCard {
                    Layout.fillWidth: true
                    Layout.minimumHeight: 196
                    title: "Harmonic Lab"
                    value: spectra.harmonicReady || spectra.fourierFrameReady ? "Models ready" : "Not ready"
                    detail: spectra.harmonicReady
                        ? spectra.detectedHarmonicCount + " detected harmonics"
                        : (spectra.fourierFrameReady
                            ? spectra.fourierSelectedComponents + " Fourier components"
                            : "Analyze a region first")
                    statusText: spectra.harmonicReady || spectra.fourierFrameReady ? "READY" : "WAIT"
                    stateColor: spectra.harmonicReady || spectra.fourierFrameReady
                        ? theme.success
                        : theme.quiet
                    onActivated: spectra.setCurrentPage(3)
                }

                WorkspaceCard {
                    Layout.fillWidth: true
                    Layout.minimumHeight: 196
                    title: "Spectrogram"
                    value: spectra.fullFileProcessing
                        ? Math.round(spectra.fullFileProgress * 100) + "%"
                        : (spectra.fullFileReady
                            ? spectra.fullFileSelectedComponents + " components"
                            : (spectra.sourceLoaded ? "Source loaded" : "No source"))
                    detail: spectra.fullFileReady
                        ? Math.round(spectra.fullFileRetainedEnergy * 100) + "% energy retained"
                        : (spectra.sourceLoaded
                            ? "Build a global FFT or STFT model"
                            : "Import audio first")
                    statusText: spectra.fullFileProcessing
                        ? "BUILDING"
                        : (spectra.fullFileReady
                            ? "READY"
                            : (spectra.sourceLoaded ? "SOURCE" : "EMPTY"))
                    stateColor: spectra.fullFileProcessing
                        ? theme.accent
                        : (spectra.fullFileReady
                            ? theme.success
                            : (spectra.sourceLoaded ? theme.warning : theme.quiet))
                    onActivated: spectra.setCurrentPage(4)
                }
            }
        }
    }
}
