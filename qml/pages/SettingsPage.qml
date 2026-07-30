import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
    id: root

    clip: true
    contentWidth: availableWidth
    contentHeight: pageLayout.height
    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

    Theme {
        id: theme
    }

    function memoryLabel(bytes) {
        const megabytes = bytes / (1024 * 1024)
        return megabytes >= 1024
            ? (megabytes / 1024).toFixed(1) + " GB"
            : Math.round(megabytes) + " MB"
    }

    GridLayout {
        id: pageLayout
        width: root.availableWidth
        height: Math.max(implicitHeight, root.availableHeight)
        columns: width < 980 ? 1 : 2
        columnSpacing: theme.space2
        rowSpacing: theme.space2

        Panel {
            Layout.fillWidth: true
            Layout.fillHeight: true
            title: "Appearance"
            subtitle: "Text size across workspaces"

            ColumnLayout {
                Layout.fillWidth: true
                spacing: theme.space2

                Text {
                    Layout.fillWidth: true
                    text: "Text scale"
                    color: theme.text
                    font.family: theme.headingFamily
                    font.pixelSize: theme.fontSize(16)
                    font.weight: Font.DemiBold
                }

                Text {
                    Layout.fillWidth: true
                    text: "Text scale applies to labels and controls throughout the app."
                    color: theme.muted
                    font.family: theme.bodyFamily
                    font.pixelSize: theme.fontSize(12)
                    wrapMode: Text.Wrap
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: theme.space1

                    AppButton {
                        text: "−"
                        enabled: spectra.textScale > 0.90
                        onClicked: spectra.setTextScale(spectra.textScale - 0.05)
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 40
                        radius: 4
                        color: theme.canvas
                        border.width: 1
                        border.color: theme.border

                        Text {
                            anchors.centerIn: parent
                            text: Math.round(spectra.textScale * 100) + "%"
                            color: theme.text
                            font.family: theme.headingFamily
                            font.pixelSize: theme.fontSize(12)
                            font.weight: Font.DemiBold
                        }
                    }

                    AppButton {
                        text: "Reset"
                        quiet: true
                        onClicked: spectra.setTextScale(1.10)
                    }

                    AppButton {
                        text: "+"
                        enabled: spectra.textScale < 1.40
                        onClicked: spectra.setTextScale(spectra.textScale + 0.05)
                    }
                }

                Item {
                    Layout.fillHeight: true
                }
            }
        }

        Panel {
            Layout.fillWidth: true
            Layout.fillHeight: true
            title: "Whole-file FFT"
            subtitle: "Memory ceiling for reusable global models"

            ColumnLayout {
                Layout.fillWidth: true
                spacing: theme.space2

                Text {
                    Layout.fillWidth: true
                    text: "Current limit  "
                        + root.memoryLabel(
                            spectra.fullFileMemoryLimitBytes)
                    color: theme.text
                    font.family: theme.headingFamily
                    font.pixelSize: theme.fontSize(16)
                    font.weight: Font.DemiBold
                }

                Text {
                    Layout.fillWidth: true
                    text: "Larger limits allow longer stereo FFT models. The estimate is shown before building."
                    color: theme.muted
                    font.family: theme.bodyFamily
                    font.pixelSize: theme.fontSize(12)
                    wrapMode: Text.Wrap
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: 4
                    columnSpacing: theme.space1
                    rowSpacing: theme.space1

                    Repeater {
                        model: [
                            {
                                "label": "512 MB",
                                "bytes": 512 * 1024 * 1024
                            },
                            {
                                "label": "768 MB",
                                "bytes": 768 * 1024 * 1024
                            },
                            {
                                "label": "1 GB",
                                "bytes": 1024 * 1024 * 1024
                            },
                            {
                                "label": "1.5 GB",
                                "bytes": 1536 * 1024 * 1024
                            }
                        ]

                        AppButton {
                            required property var modelData

                            Layout.fillWidth: true
                            text: modelData.label
                            quiet:
                                spectra.fullFileMemoryLimitBytes
                                    !== modelData.bytes
                            onClicked:
                                spectra.setFullFileMemoryLimitBytes(
                                    modelData.bytes)
                        }
                    }
                }

                StatusRow {
                    Layout.fillWidth: true
                    interactive: false
                    label: "Reconstruction cache"
                    value: spectra.fullFileCacheEntries
                        + " / 6 entries  |  256 MB"
                    stateColor: spectra.fullFileCacheEntries > 0
                        ? theme.success
                        : theme.quiet
                }

                Item {
                    Layout.fillHeight: true
                }
            }
        }

        Panel {
            Layout.fillWidth: true
            Layout.fillHeight: true
            title: "Audio runtime"
            subtitle: "Device and playback status"

            ColumnLayout {
                Layout.fillWidth: true
                spacing: theme.space2

                StatusRow {
                    Layout.fillWidth: true
                    interactive: false
                    label: "Audio device"
                    value: spectra.audioReady ? "Ready" : "Unavailable"
                    stateColor: spectra.audioReady ? theme.success : theme.warning
                }

                StatusRow {
                    Layout.fillWidth: true
                    interactive: false
                    label: "Playback"
                    value: spectra.synthPlaying
                        || spectra.sourcePlaying
                        || spectra.regionPlaying
                        || spectra.harmonicPlaying
                        || spectra.framePlaying
                        || spectra.fourierPlaying
                        || spectra.fullFilePlaying
                        ? "Playing"
                        : "Stopped"
                    stateColor: value === "Playing" ? theme.accent : theme.quiet
                }

                StatusRow {
                    Layout.fillWidth: true
                    interactive: false
                    label: "Source"
                    value: spectra.sourceLoaded ? "Loaded" : "None"
                    stateColor: spectra.sourceLoaded ? theme.success : theme.quiet
                }

                Item {
                    Layout.fillHeight: true
                }
            }
        }

        Panel {
            Layout.fillWidth: true
            Layout.fillHeight: true
            title: "Current source"
            subtitle: spectra.sourceLoaded ? spectra.sourceFileName : "No audio imported"

            ColumnLayout {
                Layout.fillWidth: true
                spacing: theme.space2

                StatusRow {
                    Layout.fillWidth: true
                    interactive: false
                    label: "Duration"
                    value: spectra.sourceLoaded
                        ? spectra.sourceDuration.toFixed(2) + " s"
                        : "—"
                    stateColor: spectra.sourceLoaded ? theme.success : theme.quiet
                }

                StatusRow {
                    Layout.fillWidth: true
                    interactive: false
                    label: "Sample rate"
                    value: spectra.sourceLoaded
                        ? spectra.sourceSampleRate.toLocaleString(Qt.locale(), "f", 0) + " Hz"
                        : "—"
                    stateColor: spectra.sourceLoaded ? theme.success : theme.quiet
                }

                StatusRow {
                    Layout.fillWidth: true
                    interactive: false
                    label: "Channels"
                    value: spectra.sourceLoaded
                        ? spectra.sourceChannels
                            + (spectra.sourceChannels === 1 ? " (mono)" : " (stereo)")
                        : "—"
                    stateColor: spectra.sourceLoaded ? theme.success : theme.quiet
                }

                Item {
                    Layout.fillHeight: true
                }
            }
        }

        Panel {
            Layout.fillWidth: true
            Layout.fillHeight: true
            title: "Analysis defaults"
            subtitle: "FFT and pitch-detection parameters"

            ColumnLayout {
                Layout.fillWidth: true
                spacing: theme.space1

                StatusRow {
                    Layout.fillWidth: true
                    interactive: false
                    label: "FFT size"
                    value: "16,384 samples"
                }

                StatusRow {
                    Layout.fillWidth: true
                    interactive: false
                    label: "Window"
                    value: "Hann"
                }

                StatusRow {
                    Layout.fillWidth: true
                    interactive: false
                    label: "Pitch range"
                    value: "40–1,200 Hz"
                }

                StatusRow {
                    Layout.fillWidth: true
                    interactive: false
                    label: "Peak threshold"
                    value: "−55 dB"
                }

                Item {
                    Layout.fillHeight: true
                }
            }
        }

        Panel {
            Layout.fillWidth: true
            Layout.fillHeight: true
            title: "Keyboard"
            subtitle: "Workspace and window shortcuts"

            ColumnLayout {
                Layout.fillWidth: true
                spacing: theme.space1

                StatusRow {
                    Layout.fillWidth: true
                    interactive: false
                    label: "Workspaces"
                    value: "1–5"
                }

                StatusRow {
                    Layout.fillWidth: true
                    interactive: false
                    label: "Full screen"
                    value: "F11"
                }

                StatusRow {
                    Layout.fillWidth: true
                    interactive: false
                    label: "Help Center"
                    value: "F1"
                }

                StatusRow {
                    Layout.fillWidth: true
                    interactive: false
                    label: "Play synth"
                    value: "Space"
                }

                Item {
                    Layout.fillHeight: true
                }
            }
        }
    }
}
