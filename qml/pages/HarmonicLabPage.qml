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

    function choosePassageDuration(duration) {
        const bounded = Math.min(
            duration, spectra.sourceDuration)
        if (spectra.regionStart + bounded
                > spectra.sourceDuration) {
            spectra.setRegionStart(Math.max(
                0, spectra.sourceDuration - bounded))
        }
        spectra.setRegionDuration(bounded)
    }

    ScrollView {
        id: scrollView

        anchors.fill: parent
        clip: true
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ColumnLayout {
            width: scrollView.availableWidth
            spacing: theme.space2

            Panel {
                Layout.fillWidth: true
                visible: !spectra.sourceLoaded
                title: "Import audio to begin"
                subtitle: "Use Import audio in the top bar to compare two simplified reconstructions"
            }

            Panel {
                Layout.fillWidth: true
                visible: spectra.sourceLoaded
                title: "1  Choose a passage"
                subtitle: "Both models reconstruct this actual span of audio—nothing is looped"

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: theme.space1

                    RowLayout {
                        Layout.fillWidth: true

                        Text {
                            text: "Start  "
                                + spectra.regionStart.toFixed(2)
                                + " s"
                            color: theme.text
                            font.family: theme.headingFamily
                            font.pixelSize: theme.fontSize(11)
                            font.weight: Font.DemiBold
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        Text {
                            text: "Selected  "
                                + spectra.regionDuration.toFixed(2)
                                + " s"
                            color: theme.muted
                            font.family: theme.bodyFamily
                            font.pixelSize: theme.fontSize(10.5)
                        }
                    }

                    Slider {
                        Layout.fillWidth: true
                        from: 0
                        to: Math.max(
                            0.01,
                            spectra.sourceDuration
                                - spectra.regionDuration)
                        value: spectra.regionStart
                        onMoved: spectra.setRegionStart(value)
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: theme.space1

                        Text {
                            text: "Length"
                            color: theme.muted
                            font.family: theme.bodyFamily
                            font.pixelSize: theme.fontSize(10.5)
                        }

                        Repeater {
                            model: [1, 2, 4, 8]

                            AppButton {
                                required property int modelData

                                Layout.preferredWidth: 64
                                text: modelData + " s"
                                quiet: Math.abs(
                                    spectra.regionDuration
                                        - modelData) > 0.01
                                enabled: spectra.sourceDuration
                                    >= modelData
                                onClicked:
                                    root.choosePassageDuration(
                                        modelData)
                            }
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        AppButton {
                            Layout.preferredWidth: 190
                            text: spectra.analysisReady
                                ? "Rebuild comparison"
                                : "Build comparison"
                            onClicked: spectra.analyzeRegion()
                        }
                    }
                }
            }

            Text {
                Layout.fillWidth: true
                visible: spectra.analysisReady
                text: "2  Compare the results"
                color: theme.text
                font.family: theme.headingFamily
                font.pixelSize: theme.fontSize(16)
                font.weight: Font.DemiBold
            }

            GridLayout {
                Layout.fillWidth: true
                visible: spectra.analysisReady
                columns: width < 900 ? 1 : 3
                columnSpacing: 12
                rowSpacing: 12

                Panel {
                    Layout.fillWidth: true
                    title: "Original passage"
                    subtitle: spectra.regionStart.toFixed(2)
                        + "–"
                        + (spectra.regionStart
                            + spectra.regionDuration).toFixed(2)
                        + " s  ·  Full detail"

                    AppButton {
                        Layout.fillWidth: true
                        text: "Play original"
                        onClicked: spectra.playRegion()
                    }
                }

                Panel {
                    Layout.fillWidth: true
                    title: "Harmonic-only model"
                    subtitle: spectra.harmonicReady
                        ? spectra.detectedHarmonicCount
                            + " / " + spectra.harmonicCount
                            + " partials  ·  No phase or noise"
                        : "No stable pitched model"

                    AppButton {
                        Layout.fillWidth: true
                        text: "Play harmonic model"
                        enabled: spectra.harmonicReady
                        onClicked: spectra.playHarmonicModel()
                    }
                }

                Panel {
                    Layout.fillWidth: true
                    title: "Evolving Fourier model"
                    subtitle: spectra.fourierFrameReady
                        ? "Top-"
                            + spectra.fourierSelectedComponents
                            + " bins in every short frame"
                        : "Reconstruction unavailable"

                    AppButton {
                        Layout.fillWidth: true
                        text: "Play Fourier model"
                        enabled: spectra.fourierFrameReady
                        onClicked: spectra.playFourierFrame()
                    }
                }
            }

            TransportPlayer {
                Layout.fillWidth: true
                visible: spectra.analysisReady
                title: spectra.labPlaybackTitle
                playing: spectra.regionPlaying
                    || spectra.harmonicPlaying
                    || spectra.fourierPlaying
                position: spectra.playbackPosition
                duration: spectra.playbackDuration > 0
                    ? spectra.playbackDuration
                    : spectra.regionDuration
                onToggleRequested: spectra.toggleLabPlayback()
                onStopRequested: spectra.stopPlayback()
                onSeekRequested: function(position) {
                    spectra.seekLabPlayback(position)
                }
            }

            Text {
                Layout.fillWidth: true
                visible: spectra.analysisReady
                text: "3  Inspect and refine"
                color: theme.text
                font.family: theme.headingFamily
                font.pixelSize: theme.fontSize(16)
                font.weight: Font.DemiBold
            }

            GridLayout {
                Layout.fillWidth: true
                visible: spectra.analysisReady
                columns: width < 960 ? 1 : 2
                columnSpacing: theme.space2
                rowSpacing: theme.space2

                Panel {
                    Layout.fillWidth: true
                    Layout.minimumHeight: 540
                    title: "Harmonic-only model"
                    subtitle: spectra.harmonicReady
                        ? spectra.analysisPitch + " fundamental"
                        : "This passage has no stable detected pitch"

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: theme.space1

                        Text {
                            Layout.fillWidth: true
                            text: "A steady tonal fingerprint built from the detected fundamental and its integer multiples. It intentionally removes noise, phase, transients, and changing articulation."
                            color: theme.muted
                            font.family: theme.bodyFamily
                            font.pixelSize: theme.fontSize(10.5)
                            wrapMode: Text.Wrap
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: theme.space1

                            AppButton {
                                Layout.fillWidth: true
                                text: "Play model"
                                enabled: spectra.harmonicReady
                                onClicked: spectra.playHarmonicModel()
                            }

                            AppButton {
                                Layout.fillWidth: true
                                text: "Export WAV"
                                quiet: true
                                enabled: spectra.harmonicReady
                                onClicked:
                                    root.exportHarmonicRequested()
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: theme.space1

                            Text {
                                Layout.preferredWidth: 48
                                text: "Partial"
                                color: theme.quiet
                                font.family: theme.headingFamily
                                font.pixelSize: theme.fontSize(10)
                            }
                            Text {
                                Layout.fillWidth: true
                                text: "Expected"
                                color: theme.quiet
                                font.family: theme.headingFamily
                                font.pixelSize: theme.fontSize(10)
                            }
                            Text {
                                Layout.fillWidth: true
                                text: "Detected"
                                color: theme.quiet
                                font.family: theme.headingFamily
                                font.pixelSize: theme.fontSize(10)
                            }
                            Text {
                                Layout.preferredWidth: 72
                                text: "Level"
                                color: theme.quiet
                                font.family: theme.headingFamily
                                font.pixelSize: theme.fontSize(10)
                            }
                        }

                        ListView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            Layout.minimumHeight: 320
                            clip: true
                            model: spectra.extractedHarmonics
                            spacing: theme.space1

                            delegate: Rectangle {
                                required property var modelData

                                width: ListView.view.width
                                height: 36
                                radius: 3
                                color: modelData.detected
                                    ? theme.raisedPanel
                                    : theme.canvas

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: theme.space1
                                    anchors.rightMargin: theme.space1
                                    spacing: theme.space1

                                    Text {
                                        Layout.preferredWidth: 40
                                        text: "H" + modelData.number
                                        color: modelData.detected
                                            ? theme.success
                                            : theme.quiet
                                        font.family: theme.headingFamily
                                        font.pixelSize: theme.fontSize(10.5)
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: Number(
                                            modelData.expected).toFixed(1)
                                            + " Hz"
                                        color: theme.text
                                        font.family: theme.bodyFamily
                                        font.pixelSize: theme.fontSize(10.5)
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: modelData.detected
                                            ? Number(modelData.detectedFrequency)
                                                .toFixed(1) + " Hz"
                                            : "—"
                                        color: modelData.detected
                                            ? theme.text : theme.quiet
                                        font.family: theme.bodyFamily
                                        font.pixelSize: theme.fontSize(10.5)
                                    }
                                    Text {
                                        Layout.preferredWidth: 64
                                        text: modelData.detected
                                            ? Number(modelData.db)
                                                .toFixed(1) + " dB"
                                            : "—"
                                        color: theme.muted
                                        font.family: theme.bodyFamily
                                        font.pixelSize: theme.fontSize(10.5)
                                    }
                                }
                            }
                        }
                    }
                }

                Panel {
                    Layout.fillWidth: true
                    Layout.minimumHeight: 540
                    title: "Evolving Fourier model"
                    subtitle: "Tracks change with overlapping short-time FFT frames"

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: theme.space2

                        Text {
                            Layout.fillWidth: true
                            text: "Keeps the strongest frequency bins and their phase in every frame. More bins preserve more detail, noise, and transients."
                            color: theme.muted
                            font.family: theme.bodyFamily
                            font.pixelSize: theme.fontSize(10.5)
                            wrapMode: Text.Wrap
                        }

                        Text {
                            Layout.fillWidth: true
                            text: "Detail  Top-"
                                + spectra.fourierSelectedComponents
                                + " bins per frame"
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
                            to: Math.max(
                                1,
                                spectra.fourierMaximumComponents)
                            stepSize: 1
                            value: Math.max(
                                1,
                                spectra.fourierSelectedComponents)
                        }

                        AppButton {
                            Layout.fillWidth: true
                            text: "Rebuild with Top-"
                                + Math.round(componentSlider.value)
                                + " per frame"
                            enabled: spectra.fourierFrameReady
                                && Math.round(componentSlider.value)
                                    !== spectra.fourierSelectedComponents
                            onClicked: spectra.rebuildFourierFrame(
                                Math.round(componentSlider.value))
                        }

                        Text {
                            Layout.fillWidth: true
                            text: "2,048-sample frames  ·  512-sample hop  ·  "
                                + spectra.regionDuration.toFixed(2)
                                + " s output"
                            color: theme.muted
                            font.family: theme.bodyFamily
                            font.pixelSize: theme.fontSize(10.5)
                            wrapMode: Text.Wrap
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: 1
                            color: theme.border
                        }

                        Text {
                            Layout.fillWidth: true
                            text: "Strongest bins in the center reference frame"
                            color: theme.quiet
                            font.family: theme.headingFamily
                            font.pixelSize: theme.fontSize(10)
                        }

                        Repeater {
                            model: spectra.fourierComponents

                            Rectangle {
                                required property var modelData

                                Layout.fillWidth: true
                                implicitHeight: 30
                                radius: 3
                                color: theme.canvas

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: theme.space1
                                    anchors.rightMargin: theme.space1

                                    Text {
                                        Layout.preferredWidth: 28
                                        text: modelData.rank
                                        color: theme.text
                                        font.family: theme.headingFamily
                                        font.pixelSize: theme.fontSize(10.5)
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: Number(modelData.frequency)
                                            .toFixed(1) + " Hz"
                                        color: theme.text
                                        font.family: theme.bodyFamily
                                        font.pixelSize: theme.fontSize(10.5)
                                    }
                                    Text {
                                        Layout.preferredWidth: 72
                                        text: Number(modelData.db)
                                            .toFixed(1) + " dB"
                                        color: theme.muted
                                        font.family: theme.bodyFamily
                                        font.pixelSize: theme.fontSize(10.5)
                                    }
                                    Text {
                                        Layout.preferredWidth: 86
                                        text: (Number(modelData.phase) >= 0
                                            ? "+" : "")
                                            + Number(modelData.phase)
                                                .toFixed(2)
                                            + " rad"
                                        color: theme.muted
                                        font.family: theme.bodyFamily
                                        font.pixelSize: theme.fontSize(10.5)
                                    }
                                }
                            }
                        }

                        Item {
                            Layout.fillHeight: true
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: theme.space1

                            AppButton {
                                Layout.fillWidth: true
                                text: "Play model"
                                enabled: spectra.fourierFrameReady
                                onClicked: spectra.playFourierFrame()
                            }

                            AppButton {
                                Layout.fillWidth: true
                                text: "Export WAV"
                                quiet: true
                                enabled: spectra.fourierFrameReady
                                onClicked:
                                    root.exportFourierRequested()
                            }
                        }
                    }
                }
            }
        }
    }
}
