import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
    id: root

    signal importRequested()

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
        columns: width < 1080 ? 1 : 2
        columnSpacing: theme.space2
        rowSpacing: theme.space2

        Panel {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredWidth: 440
            title: "Imported source"
            subtitle: spectra.sourceLoaded ? spectra.sourceFileName : "No audio imported"

            ColumnLayout {
                Layout.fillWidth: true
                spacing: theme.space2

                AppButton {
                    Layout.fillWidth: true
                    text: spectra.sourceLoaded ? "Replace audio" : "Import audio"
                    onClicked: root.importRequested()
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: theme.space2
                    rowSpacing: theme.space1
                    visible: spectra.sourceLoaded

                    Text {
                        text: "Duration"
                        color: theme.muted
                        font.family: theme.bodyFamily
                        font.pixelSize: theme.fontSize(11)
                    }
                    Text {
                        text: spectra.sourceDuration.toFixed(2) + " s"
                        color: theme.text
                        font.family: theme.headingFamily
                        font.pixelSize: theme.fontSize(12)
                        font.weight: Font.DemiBold
                    }
                    Text {
                        text: "Sample rate"
                        color: theme.muted
                        font.family: theme.bodyFamily
                        font.pixelSize: theme.fontSize(11)
                    }
                    Text {
                        text: spectra.sourceSampleRate + " Hz"
                        color: theme.text
                        font.family: theme.headingFamily
                        font.pixelSize: theme.fontSize(12)
                        font.weight: Font.DemiBold
                    }
                    Text {
                        text: "Channels"
                        color: theme.muted
                        font.family: theme.bodyFamily
                        font.pixelSize: theme.fontSize(11)
                    }
                    Text {
                        text: spectra.sourceChannels + (spectra.sourceChannels === 1 ? " (mono)" : " (stereo)")
                        color: theme.text
                        font.family: theme.headingFamily
                        font.pixelSize: theme.fontSize(12)
                        font.weight: Font.DemiBold
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 1
                    color: theme.border
                    visible: spectra.sourceLoaded
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: theme.space1
                    visible: spectra.sourceLoaded

                    Text {
                        text: "Region start  " + spectra.regionStart.toFixed(2) + " s"
                        color: theme.text
                        font.family: theme.headingFamily
                        font.pixelSize: theme.fontSize(12)
                        font.weight: Font.DemiBold
                    }

                    Slider {
                        Layout.fillWidth: true
                        from: 0
                        to: Math.max(0.01, spectra.sourceDuration - 0.01)
                        value: spectra.regionStart
                        onMoved: spectra.setRegionStart(value)
                    }

                    Text {
                        text: "Region duration  " + spectra.regionDuration.toFixed(2) + " s"
                        color: theme.text
                        font.family: theme.headingFamily
                        font.pixelSize: theme.fontSize(12)
                        font.weight: Font.DemiBold
                    }

                    Slider {
                        Layout.fillWidth: true
                        from: 0.01
                        to: Math.max(0.01, spectra.sourceDuration - spectra.regionStart)
                        value: spectra.regionDuration
                        onMoved: spectra.setRegionDuration(value)
                    }
                }

                AppButton {
                    Layout.fillWidth: true
                    visible: spectra.sourceLoaded
                    text: spectra.analysisReady ? "Analyze region again" : "Analyze selected region"
                    onClicked: spectra.analyzeRegion()
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: theme.space1
                    visible: spectra.sourceLoaded

                    AppButton {
                        Layout.fillWidth: true
                        text: "Play full file"
                        quiet: !spectra.sourcePlaying
                        onClicked: spectra.toggleSourcePlayback()
                    }

                    AppButton {
                        Layout.fillWidth: true
                        text: "Play region"
                        quiet: !spectra.regionPlaying
                        enabled: spectra.analysisReady
                        onClicked: spectra.toggleRegionPlayback()
                    }
                }

                TransportPlayer {
                    Layout.fillWidth: true
                    visible: spectra.sourceLoaded
                    title: spectra.regionActive ? "Analyzed region" : "Original full file"
                    playing: spectra.sourcePlaying || spectra.regionPlaying
                    position: spectra.playbackPosition
                    duration: spectra.regionActive ? spectra.regionDuration : spectra.sourceDuration
                    onToggleRequested: {
                        if (spectra.regionActive)
                            spectra.toggleRegionPlayback()
                        else
                            spectra.toggleSourcePlayback()
                    }
                    onStopRequested: spectra.stopPlayback()
                }

                Text {
                    Layout.fillWidth: true
                    text: spectra.statusText
                    color: theme.muted
                    font.family: theme.bodyFamily
                    font.pixelSize: theme.fontSize(12)
                    wrapMode: Text.Wrap
                }

                Item {
                    Layout.fillHeight: true
                }
            }
        }

        Panel {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredWidth: 780
            title: "Region visualization"
            subtitle: spectra.analysisReady
                ? spectra.analysisPeakCount + " peaks | " + spectra.analysisPitch
                : "Choose a region and run analysis"

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: theme.space2

                Text {
                    text: "Waveform and selected region"
                    color: theme.text
                    font.family: theme.headingFamily
                    font.pixelSize: theme.fontSize(16)
                    font.weight: Font.DemiBold
                }

                Canvas {
                    id: sourceWaveform
                    Layout.fillWidth: true
                    Layout.preferredHeight: 220

                    onPaint: {
                        const ctx = getContext("2d")
                        ctx.clearRect(0, 0, width, height)
                        ctx.fillStyle = theme.canvas
                        ctx.fillRect(0, 0, width, height)
                        ctx.strokeStyle = theme.border
                        ctx.strokeRect(0.5, 0.5, width - 1, height - 1)

                        if (spectra.sourceLoaded && spectra.sourceDuration > 0) {
                            const regionX = spectra.regionStart / spectra.sourceDuration * width
                            const regionWidth = spectra.regionDuration / spectra.sourceDuration * width
                            ctx.fillStyle = Qt.rgba(theme.accent.r, theme.accent.g, theme.accent.b, 0.13)
                            ctx.fillRect(regionX, 1, regionWidth, height - 2)
                            ctx.strokeStyle = theme.accent
                            ctx.strokeRect(regionX + 0.5, 1.5, Math.max(1, regionWidth - 1), height - 3)
                        }

                        const minimums = spectra.sourceWaveformMinimums
                        const maximums = spectra.sourceWaveformMaximums
                        if (!minimums || minimums.length < 2 || minimums.length !== maximums.length)
                            return

                        ctx.strokeStyle = theme.muted
                        ctx.lineWidth = 1
                        for (let i = 0; i < minimums.length; ++i) {
                            const x = i * width / (minimums.length - 1)
                            const y1 = height / 2 - Number(maximums[i]) * height * 0.44
                            const y2 = height / 2 - Number(minimums[i]) * height * 0.44
                            ctx.beginPath()
                            ctx.moveTo(x, y1)
                            ctx.lineTo(x, y2)
                            ctx.stroke()
                        }
                    }

                    Connections {
                        target: spectra
                        function onSourceChanged() {
                            sourceWaveform.requestPaint()
                        }
                        function onAnalysisChanged() {
                            sourceWaveform.requestPaint()
                        }
                    }
                }

                Text {
                    text: "Magnitude spectrum"
                    color: theme.text
                    font.family: theme.headingFamily
                    font.pixelSize: theme.fontSize(16)
                    font.weight: Font.DemiBold
                }

                Canvas {
                    id: analysisSpectrumCanvas
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumHeight: 220

                    onPaint: {
                        const ctx = getContext("2d")
                        ctx.clearRect(0, 0, width, height)
                        ctx.fillStyle = theme.canvas
                        ctx.fillRect(0, 0, width, height)
                        ctx.strokeStyle = theme.border
                        ctx.strokeRect(0.5, 0.5, width - 1, height - 1)
                        const values = spectra.analysisSpectrum
                        if (!values || values.length < 2)
                            return
                        ctx.strokeStyle = theme.accent
                        ctx.lineWidth = 1.5
                        ctx.beginPath()
                        for (let i = 0; i < values.length; ++i) {
                            const x = i * width / (values.length - 1)
                            const normalized = Math.max(0, Math.min(1, (Number(values[i]) + 100) / 100))
                            const y = height - normalized * (height - 8) - 4
                            if (i === 0)
                                ctx.moveTo(x, y)
                            else
                                ctx.lineTo(x, y)
                        }
                        ctx.stroke()

                        const peaks = spectra.analysisPeaks
                        const nyquist = Math.max(1, spectra.sourceSampleRate / 2)
                        ctx.fillStyle = theme.success
                        for (let peakIndex = 0; peakIndex < peaks.length; ++peakIndex) {
                            const peak = peaks[peakIndex]
                            const x = Number(peak.frequency) / nyquist * width
                            const normalized = Math.max(0, Math.min(1, (Number(peak.db) + 100) / 100))
                            const y = height - normalized * (height - 8) - 4
                            ctx.beginPath()
                            ctx.arc(x, y, 3, 0, Math.PI * 2)
                            ctx.fill()
                        }
                    }

                    Connections {
                        target: spectra
                        function onAnalysisChanged() {
                            analysisSpectrumCanvas.requestPaint()
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
                            text: "Estimated pitch"
                            color: theme.text
                            font.family: theme.headingFamily
                            font.pixelSize: theme.fontSize(16)
                            font.weight: Font.DemiBold
                        }

                        Text {
                            text: spectra.analysisPitch
                            color: theme.text
                            font.family: theme.headingFamily
                            font.pixelSize: theme.fontSize(20)
                            font.weight: Font.DemiBold
                        }

                        Text {
                            text: spectra.analysisNote
                            color: theme.muted
                            font.family: theme.bodyFamily
                            font.pixelSize: theme.fontSize(12)
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: theme.space1

                        Text {
                            text: "Analysis confidence"
                            color: theme.text
                            font.family: theme.headingFamily
                            font.pixelSize: theme.fontSize(16)
                            font.weight: Font.DemiBold
                        }

                        Text {
                            text: Math.round(spectra.analysisConfidence * 100) + "%"
                            color: spectra.analysisConfidence >= 0.9 ? theme.success : theme.warning
                            font.family: theme.headingFamily
                            font.pixelSize: theme.fontSize(20)
                            font.weight: Font.DemiBold
                        }

                        Text {
                            text: spectra.analysisPeakCount + " interpolated spectral peaks"
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
