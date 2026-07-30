import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
    id: root

    signal exportRequested()

    clip: true
    contentWidth: availableWidth
    contentHeight: pageLayout.height
    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

    Theme {
        id: theme
    }

    GridLayout {
        id: pageLayout
        width: root.availableWidth
        height: Math.max(implicitHeight, root.availableHeight)
        columns: width < 1120 ? 1 : 2
        columnSpacing: theme.space2
        rowSpacing: theme.space2

        Panel {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredWidth: 500
            title: "Harmonic Synth"
            subtitle: spectra.statusText

            ColumnLayout {
                Layout.fillWidth: true
                spacing: theme.space2

                RowLayout {
                    Layout.fillWidth: true
                    spacing: theme.space1

                    AppButton {
                        Layout.fillWidth: true
                        text: "Play from start"
                        onClicked: spectra.playSynth()
                    }

                    AppButton {
                        Layout.fillWidth: true
                        text: "Export WAV"
                        quiet: true
                        onClicked: root.exportRequested()
                    }
                }

                TransportPlayer {
                    Layout.fillWidth: true
                    title: spectra.synthFrequency.toFixed(1) + " Hz " + spectra.synthPresetName + " tone"
                    playing: spectra.synthPlaying
                    position: spectra.playbackPosition
                    duration: spectra.synthDuration
                    onToggleRequested: spectra.toggleSynthPlayback()
                    onStopRequested: spectra.stopPlayback()
                    onSeekRequested: function(position) {
                        spectra.seekSynthPlayback(position)
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: theme.space1

                    Text {
                        text: "Fundamental frequency  " + spectra.synthFrequency.toFixed(1) + " Hz"
                        color: theme.text
                        font.family: theme.headingFamily
                        font.pixelSize: theme.fontSize(12)
                        font.weight: Font.DemiBold
                    }

                    Slider {
                        Layout.fillWidth: true
                        from: 40
                        to: 1200
                        value: spectra.synthFrequency
                        onMoved: spectra.setSynthFrequency(value)
                    }

                    Text {
                        text: "Duration  " + spectra.synthDuration.toFixed(2) + " s"
                        color: theme.text
                        font.family: theme.headingFamily
                        font.pixelSize: theme.fontSize(12)
                        font.weight: Font.DemiBold
                    }

                    Slider {
                        Layout.fillWidth: true
                        from: 0.2
                        to: 3
                        value: spectra.synthDuration
                        onMoved: spectra.setSynthDuration(value)
                    }

                    Text {
                        text: "Master gain  " + Math.round(spectra.synthGain * 100) + "%"
                        color: theme.text
                        font.family: theme.headingFamily
                        font.pixelSize: theme.fontSize(12)
                        font.weight: Font.DemiBold
                    }

                    Slider {
                        Layout.fillWidth: true
                        from: 0.05
                        to: 1
                        value: spectra.synthGain
                        onMoved: spectra.setSynthGain(value)
                    }
                }

                Text {
                    text: "Presets"
                    color: theme.text
                    font.family: theme.headingFamily
                    font.pixelSize: theme.fontSize(16)
                    font.weight: Font.DemiBold
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: 3
                    columnSpacing: theme.space1
                    rowSpacing: theme.space1

                    Repeater {
                        model: ["Sine", "Square-like", "Saw-like", "Clarinet-like", "Bright string"]

                        AppButton {
                            required property int index
                            required property string modelData
                            Layout.fillWidth: true
                            text: modelData
                            quiet: spectra.synthPreset !== index
                            onClicked: spectra.applySynthPreset(index)
                        }
                    }
                }

                Text {
                    text: "Harmonics"
                    color: theme.text
                    font.family: theme.headingFamily
                    font.pixelSize: theme.fontSize(16)
                    font.weight: Font.DemiBold
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: 4
                    columnSpacing: theme.space1
                    rowSpacing: theme.space1

                    Repeater {
                        model: 16

                        ColumnLayout {
                            required property int index
                            Layout.fillWidth: true
                            spacing: theme.space1

                            Text {
                                text: "H" + (parent.index + 1)
                                color: theme.muted
                                font.family: theme.bodyFamily
                                font.pixelSize: theme.fontSize(10.5)
                            }

                            Slider {
                                Layout.fillWidth: true
                                from: 0
                                to: 1
                                value: spectra.harmonicAmplitudes[parent.index] || 0
                                onMoved: spectra.setHarmonicAmplitude(parent.index, value)
                            }
                        }
                    }
                }
            }
        }

        Panel {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredWidth: 760
            title: "Visualization"
            subtitle: "Generated waveform and magnitude spectrum"

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: theme.space2

                Text {
                    text: "Waveform"
                    color: theme.text
                    font.family: theme.headingFamily
                    font.pixelSize: theme.fontSize(16)
                    font.weight: Font.DemiBold
                }

                Canvas {
                    id: waveformCanvas
                    Layout.fillWidth: true
                    Layout.preferredHeight: 180

                    onPaint: {
                        const ctx = getContext("2d")
                        ctx.clearRect(0, 0, width, height)
                        ctx.fillStyle = theme.canvas
                        ctx.fillRect(0, 0, width, height)
                        ctx.strokeStyle = theme.border
                        ctx.strokeRect(0.5, 0.5, width - 1, height - 1)
                        ctx.strokeStyle = theme.quiet
                        ctx.beginPath()
                        ctx.moveTo(0, height / 2)
                        ctx.lineTo(width, height / 2)
                        ctx.stroke()
                        const minimums = spectra.synthWaveformMinimums
                        const maximums = spectra.synthWaveformMaximums
                        if (!minimums || !maximums
                                || minimums.length < 2
                                || minimums.length !== maximums.length)
                            return
                        ctx.strokeStyle = theme.accent
                        ctx.lineWidth = 1
                        ctx.beginPath()
                        for (let i = 0; i < minimums.length; ++i) {
                            const x = i * width / (minimums.length - 1)
                            const minimumY = height / 2
                                - Number(minimums[i]) * height * 0.42
                            const maximumY = height / 2
                                - Number(maximums[i]) * height * 0.42
                            ctx.moveTo(x, minimumY)
                            ctx.lineTo(x, maximumY)
                        }
                        ctx.stroke()
                    }

                    Connections {
                        target: spectra
                        function onSynthVisualizationChanged() {
                            waveformCanvas.requestPaint()
                        }
                    }
                }

                Text {
                    text: "Frequency spectrum"
                    color: theme.text
                    font.family: theme.headingFamily
                    font.pixelSize: theme.fontSize(16)
                    font.weight: Font.DemiBold
                }

                Canvas {
                    id: spectrumCanvas
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumHeight: 180

                    onPaint: {
                        const ctx = getContext("2d")
                        ctx.clearRect(0, 0, width, height)
                        ctx.fillStyle = theme.canvas
                        ctx.fillRect(0, 0, width, height)
                        ctx.strokeStyle = theme.border
                        ctx.strokeRect(0.5, 0.5, width - 1, height - 1)
                        const values = spectra.synthSpectrum
                        if (!values || values.length < 2)
                            return
                        ctx.strokeStyle = theme.accent
                        ctx.lineWidth = 1.5
                        ctx.beginPath()
                        let started = false
                        for (let i = 0; i < values.length; ++i) {
                            const frequency = i / values.length * 22050
                            if (frequency > 8000)
                                break
                            const x = frequency / 8000 * width
                            const normalized = Math.max(0, Math.min(1, (Number(values[i]) + 80) / 80))
                            const y = height - normalized * (height - 8) - 4
                            if (!started) {
                                ctx.moveTo(x, y)
                                started = true
                            } else {
                                ctx.lineTo(x, y)
                            }
                        }
                        ctx.stroke()

                        const peaks = spectra.synthPeaks
                        ctx.fillStyle = theme.danger
                        for (let peakIndex = 0;
                                peakIndex < peaks.length;
                                ++peakIndex) {
                            const peak = peaks[peakIndex]
                            const frequency = Number(peak.frequency)
                            if (frequency > 8000)
                                continue
                            const x = frequency / 8000 * width
                            const normalized = Math.max(
                                0,
                                Math.min(
                                    1,
                                    (Number(peak.db) + 80) / 80))
                            const y =
                                height - normalized * (height - 8) - 4
                            ctx.beginPath()
                            ctx.arc(x, y, 4, 0, Math.PI * 2)
                            ctx.fill()
                        }
                    }

                    Connections {
                        target: spectra
                        function onSynthVisualizationChanged() {
                            spectrumCanvas.requestPaint()
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: theme.space3

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: theme.space1

                        Text {
                            text: "Detected peaks"
                            color: theme.text
                            font.family: theme.headingFamily
                            font.pixelSize: theme.fontSize(16)
                            font.weight: Font.DemiBold
                        }

                        Text {
                            text: spectra.synthPeakCount
                                + " spectral peaks above -55 dB"
                            color: theme.muted
                            font.family: theme.bodyFamily
                            font.pixelSize: theme.fontSize(12)
                        }

                        Repeater {
                            model: Math.min(5, spectra.synthPeaks.length)

                            Text {
                                required property int index

                                readonly property var peak:
                                    spectra.synthPeaks[index]

                                text: (index + 1) + ".  "
                                    + Number(peak.frequency).toFixed(1)
                                    + " Hz   "
                                    + Number(peak.db).toFixed(1)
                                    + " dB"
                                color: theme.muted
                                font.family: theme.bodyFamily
                                font.pixelSize: theme.fontSize(11)
                            }
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: theme.space1

                        Text {
                            text: "Estimated pitch"
                            color: theme.text
                            font.family: theme.headingFamily
                            font.pixelSize: theme.fontSize(16)
                            font.weight: Font.DemiBold
                        }

                        Text {
                            text: spectra.synthPitch
                            color: theme.text
                            font.family: theme.headingFamily
                            font.pixelSize: theme.fontSize(20)
                            font.weight: Font.DemiBold
                        }

                        Text {
                            text: spectra.synthNote
                            color: theme.muted
                            font.family: theme.bodyFamily
                            font.pixelSize: theme.fontSize(12)
                        }
                    }
                }
            }
        }
    }
}
