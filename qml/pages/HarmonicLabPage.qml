import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    signal exportHarmonicRequested()
    signal exportFourierRequested()

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

            GridLayout {
                Layout.fillWidth: true
                columns: width < 1000 ? 2 : 4
                columnSpacing: 12
                rowSpacing: 12

                Panel {
                    Layout.fillWidth: true
                    title: "Original region"
                    subtitle: spectra.analysisReady
                        ? spectra.regionStart.toFixed(2) + "–"
                            + (spectra.regionStart + spectra.regionDuration).toFixed(2) + " s"
                        : "No analyzed region"

                    AppButton {
                        Layout.fillWidth: true
                        text: "Play region"
                        enabled: spectra.analysisReady
                        onClicked: spectra.playRegion()
                    }
                }

                Panel {
                    Layout.fillWidth: true
                    title: "Harmonic model"
                    subtitle: spectra.harmonicReady
                        ? spectra.detectedHarmonicCount + " / " + spectra.harmonicCount + " detected"
                        : "No stable pitched model"

                    AppButton {
                        Layout.fillWidth: true
                        text: "Play harmonics"
                        enabled: spectra.harmonicReady
                        onClicked: spectra.playHarmonicModel()
                    }
                }

                Panel {
                    Layout.fillWidth: true
                    title: "Original FFT frame"
                    subtitle: spectra.fourierFrameReady
                        ? spectra.fourierMaximumComponents + " ranked components"
                        : "Frame unavailable"

                    AppButton {
                        Layout.fillWidth: true
                        text: "Play frame"
                        enabled: spectra.fourierFrameReady
                        onClicked: spectra.playOriginalFrame()
                    }
                }

                Panel {
                    Layout.fillWidth: true
                    title: "Fourier frame"
                    subtitle: spectra.fourierFrameReady
                        ? "Top-" + spectra.fourierSelectedComponents + " with phase"
                        : "Frame unavailable"

                    AppButton {
                        Layout.fillWidth: true
                        text: "Play Fourier"
                        enabled: spectra.fourierFrameReady
                        onClicked: spectra.playFourierFrame()
                    }
                }
            }

            GridLayout {
                Layout.fillWidth: true
                columns: width < 960 ? 1 : 2
                columnSpacing: theme.space2
                rowSpacing: theme.space2

                Panel {
                    Layout.fillWidth: true
                    Layout.minimumHeight: 500
                    title: "Integer-harmonic reconstruction"
                    subtitle: spectra.harmonicReady
                        ? spectra.analysisPitch + " fundamental"
                        : "Analyze a pitched region first"

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: theme.space1

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: theme.space1

                            AppButton {
                                Layout.fillWidth: true
                                text: "Play reconstruction"
                                enabled: spectra.harmonicReady
                                onClicked: spectra.playHarmonicModel()
                            }

                            AppButton {
                                Layout.fillWidth: true
                                text: "Export WAV"
                                quiet: true
                                enabled: spectra.harmonicReady
                                onClicked: root.exportHarmonicRequested()
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: theme.space1

                            Text {
                                Layout.preferredWidth: 48
                                text: "#"
                                color: theme.quiet
                                font.family: theme.headingFamily
                                font.pixelSize: theme.fontSize(10.5)
                                font.weight: Font.DemiBold
                            }
                            Text {
                                Layout.fillWidth: true
                                text: "Expected"
                                color: theme.quiet
                                font.family: theme.headingFamily
                                font.pixelSize: theme.fontSize(10.5)
                                font.weight: Font.DemiBold
                            }
                            Text {
                                Layout.fillWidth: true
                                text: "Detected"
                                color: theme.quiet
                                font.family: theme.headingFamily
                                font.pixelSize: theme.fontSize(10.5)
                                font.weight: Font.DemiBold
                            }
                            Text {
                                Layout.preferredWidth: 80
                                text: "Level"
                                color: theme.quiet
                                font.family: theme.headingFamily
                                font.pixelSize: theme.fontSize(10.5)
                                font.weight: Font.DemiBold
                            }
                        }

                        ListView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            model: spectra.extractedHarmonics
                            spacing: theme.space1

                            delegate: Rectangle {
                                required property var modelData
                                width: ListView.view.width
                                height: 36
                                radius: 3
                                color: modelData.detected ? theme.raisedPanel : theme.canvas

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: theme.space1
                                    anchors.rightMargin: theme.space1
                                    spacing: theme.space1

                                    Text {
                                        Layout.preferredWidth: 40
                                        text: "H" + modelData.number
                                        color: modelData.detected ? theme.success : theme.quiet
                                        font.family: theme.headingFamily
                                        font.pixelSize: theme.fontSize(11)
                                        font.weight: Font.DemiBold
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: Number(modelData.expected).toFixed(1) + " Hz"
                                        color: theme.text
                                        font.family: theme.bodyFamily
                                        font.pixelSize: theme.fontSize(11)
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: modelData.detected
                                            ? Number(modelData.detectedFrequency).toFixed(1) + " Hz"
                                            : "—"
                                        color: modelData.detected ? theme.text : theme.quiet
                                        font.family: theme.bodyFamily
                                        font.pixelSize: theme.fontSize(11)
                                    }
                                    Text {
                                        Layout.preferredWidth: 72
                                        text: modelData.detected ? Number(modelData.db).toFixed(1) + " dB" : "—"
                                        color: theme.muted
                                        font.family: theme.bodyFamily
                                        font.pixelSize: theme.fontSize(11)
                                    }
                                }
                            }
                        }
                    }
                }

                Panel {
                    Layout.fillWidth: true
                    Layout.minimumHeight: 500
                    title: "Phase-preserving Fourier frame"
                    subtitle: "Ranked complex FFT coefficients"

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: theme.space2

                        Text {
                            Layout.fillWidth: true
                            text: "Selected components  " + spectra.fourierSelectedComponents
                            color: theme.text
                            font.family: theme.headingFamily
                            font.pixelSize: theme.fontSize(14)
                            font.weight: Font.DemiBold
                        }

                        Slider {
                            id: componentSlider
                            Layout.fillWidth: true
                            enabled: spectra.fourierFrameReady
                            from: 1
                            to: Math.max(1, spectra.fourierMaximumComponents)
                            stepSize: 1
                            value: Math.max(1, spectra.fourierSelectedComponents)
                        }

                        AppButton {
                            Layout.fillWidth: true
                            text: "Rebuild Top-" + Math.round(componentSlider.value)
                            enabled: spectra.fourierFrameReady
                                && Math.round(componentSlider.value) !== spectra.fourierSelectedComponents
                            onClicked: spectra.rebuildFourierFrame(Math.round(componentSlider.value))
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: 1
                            color: theme.border
                        }

                        StatusRow {
                            Layout.fillWidth: true
                            interactive: false
                            label: "FFT frame"
                            value: spectra.fourierFrameReady ? "Ready" : "Unavailable"
                            stateColor: spectra.fourierFrameReady ? theme.success : theme.quiet
                        }

                        StatusRow {
                            Layout.fillWidth: true
                            interactive: false
                            label: "Components"
                            value: spectra.fourierSelectedComponents + " / " + spectra.fourierMaximumComponents
                            stateColor: spectra.fourierFrameReady ? theme.success : theme.quiet
                        }

                        StatusRow {
                            Layout.fillWidth: true
                            interactive: false
                            label: "Phase"
                            value: "Preserved per complex coefficient"
                            stateColor: spectra.fourierFrameReady ? theme.success : theme.quiet
                        }

                        Item {
                            Layout.fillHeight: true
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: theme.space1

                            AppButton {
                                Layout.fillWidth: true
                                text: "Play Fourier frame"
                                enabled: spectra.fourierFrameReady
                                onClicked: spectra.playFourierFrame()
                            }

                            AppButton {
                                Layout.fillWidth: true
                                text: "Export WAV"
                                quiet: true
                                enabled: spectra.fourierFrameReady
                                onClicked: root.exportFourierRequested()
                            }
                        }
                    }
                }
            }
        }
    }
}
