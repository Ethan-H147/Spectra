import QtQuick
import QtQuick.Layouts
import Spectra.Native

ColumnLayout {
    id: root

    property bool compactMode: false

    spacing: theme.space1

    Theme {
        id: theme
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: root.compactMode ? theme.space1 : theme.space2

        RowLayout {
            spacing: 6

            Rectangle {
                implicitWidth: 18
                implicitHeight: 3
                radius: 2
                color: theme.muted
            }

            Text {
                text: "Original"
                color: theme.muted
                font.family: theme.bodyFamily
                font.pixelSize: theme.fontSize(10)
            }
        }

        RowLayout {
            spacing: 6

            Rectangle {
                implicitWidth: 18
                implicitHeight: 3
                radius: 2
                color: theme.accent
            }

            Text {
                text: spectra.effectSpectrumPreview
                    ? "Predicted"
                    : "Processed"
                color: theme.text
                font.family: theme.bodyFamily
                font.pixelSize: theme.fontSize(10)
            }
        }

        Item {
            Layout.fillWidth: true
        }

        Rectangle {
            implicitWidth: spectrumStateLabel.implicitWidth
                + theme.space1 * 2
            implicitHeight: 24
            radius: 3
            color: spectra.effectSpectrumPreview
                ? Qt.rgba(0.90, 0.63, 0.24, 0.14)
                : Qt.rgba(0.27, 0.75, 0.47, 0.14)
            border.width: 1
            border.color: spectra.effectSpectrumPreview
                ? theme.warning
                : theme.success
            visible: spectra.sourceLoaded

            Text {
                id: spectrumStateLabel

                anchors.centerIn: parent
                text: spectra.effectSpectrumPreview
                    ? "PREVIEW"
                    : "MEASURED"
                color: spectra.effectSpectrumPreview
                    ? theme.warning
                    : theme.success
                font.family: theme.headingFamily
                font.pixelSize: theme.fontSize(9)
                font.weight: Font.DemiBold
            }
        }
    }

    Rectangle {
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumHeight: root.compactMode ? 160 : 210
        radius: 4
        color: theme.canvas
        border.width: 1
        border.color: theme.border
        clip: true

        Canvas {
            id: effectSpectrumGrid

            anchors.fill: parent

            readonly property real plotX: root.compactMode
                ? 50
                : (width < 620 ? 52 : 62)
            readonly property real plotY: 12
            readonly property real plotWidth: Math.max(
                80, width - plotX - 14)
            readonly property real plotHeight: Math.max(
                48, height - plotY - 34)
            readonly property real sourceMaximumFrequency:
                spectra.sourceLoaded
                    ? Math.max(
                        1,
                        spectra.effectSpectrumMaximumFrequency)
                    : 8000
            readonly property real peakFocus: Math.max(
                spectra.effectOriginalPeakFrequency,
                spectra.effectTransformedPeakFrequency)
            readonly property real viewMaximumFrequency:
                Math.min(
                    sourceMaximumFrequency,
                    Math.max(
                        1000,
                        peakFocus * 2.5,
                        spectra.effectMode === 2
                            ? Math.abs(
                                spectra.effectFrequencyShift)
                                * 1.5
                            : 0))

            function frequencyLabel(frequency) {
                if (frequency >= 1000)
                    return (frequency / 1000).toFixed(
                        frequency >= 10000 ? 0 : 1) + " kHz"
                return frequency.toFixed(0) + " Hz"
            }

            function xForFrequency(frequency) {
                return plotX
                    + frequency / viewMaximumFrequency
                        * plotWidth
            }

            onPaint: {
                const ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                ctx.fillStyle = theme.canvas
                ctx.fillRect(0, 0, width, height)
                ctx.font = theme.fontSize(9) + "px '"
                    + theme.bodyFamily + "'"
                ctx.lineWidth = 1

                for (let index = 0; index <= 4; ++index) {
                    const fraction = index / 4
                    const x = plotX + fraction * plotWidth
                    const frequency =
                        fraction * viewMaximumFrequency
                    ctx.strokeStyle = "#35373D"
                    ctx.beginPath()
                    ctx.moveTo(x, plotY)
                    ctx.lineTo(x, plotY + plotHeight)
                    ctx.stroke()

                    const label = frequencyLabel(frequency)
                    const metrics = ctx.measureText(label)
                    ctx.fillStyle = theme.muted
                    ctx.fillText(
                        label,
                        Math.max(
                            2,
                            Math.min(
                                width - metrics.width - 2,
                                x - metrics.width * 0.5)),
                        plotY + plotHeight + 17)
                }

                const dbValues = root.compactMode
                    ? [0, -45, -90]
                    : [0, -30, -60, -90]
                for (let index = 0;
                        index < dbValues.length;
                        ++index) {
                    const db = dbValues[index]
                    const y = plotY
                        + (0 - db) / 90 * plotHeight
                    ctx.strokeStyle = "#35373D"
                    ctx.beginPath()
                    ctx.moveTo(plotX, y)
                    ctx.lineTo(plotX + plotWidth, y)
                    ctx.stroke()
                    ctx.fillStyle = theme.muted
                    ctx.fillText(
                        db.toFixed(0) + " dB",
                        6,
                        y + theme.fontSize(9) * 0.35)
                }

                ctx.strokeStyle = theme.hoverBorder
                ctx.strokeRect(
                    plotX + 0.5,
                    plotY + 0.5,
                    plotWidth - 1,
                    plotHeight - 1)
            }

            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
        }

        SpectrumItem {
            anchors.fill: parent
            z: 1
            visible: spectra.sourceLoaded
                && spectra.effectOriginalSpectrum.length > 0
            spectrum: spectra.effectOriginalSpectrum
            spectrumMaximumFrequency:
                effectSpectrumGrid.sourceMaximumFrequency
            minimumFrequency: 0
            maximumFrequency:
                effectSpectrumGrid.viewMaximumFrequency
            minimumDb: -90
            maximumDb: 0
            plotX: effectSpectrumGrid.plotX
            plotY: effectSpectrumGrid.plotY
            plotWidth: effectSpectrumGrid.plotWidth
            plotHeight: effectSpectrumGrid.plotHeight
            lineColor: theme.muted
            peakColor: theme.muted
        }

        SpectrumItem {
            anchors.fill: parent
            z: 2
            visible: spectra.sourceLoaded
                && spectra.effectTransformedSpectrum.length > 0
            spectrum: spectra.effectTransformedSpectrum
            spectrumMaximumFrequency:
                effectSpectrumGrid.sourceMaximumFrequency
            minimumFrequency: 0
            maximumFrequency:
                effectSpectrumGrid.viewMaximumFrequency
            minimumDb: -90
            maximumDb: 0
            plotX: effectSpectrumGrid.plotX
            plotY: effectSpectrumGrid.plotY
            plotWidth: effectSpectrumGrid.plotWidth
            plotHeight: effectSpectrumGrid.plotHeight
            lineColor: theme.accent
            peakColor: theme.accent
        }

        Canvas {
            id: spectrumMovementOverlay

            anchors.fill: parent
            z: 3

            onPaint: {
                const ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                if (!spectra.sourceLoaded
                        || spectra.effectOriginalPeakFrequency <= 0
                        || spectra.effectTransformedPeakFrequency
                            <= 0) {
                    return
                }

                const maximum =
                    effectSpectrumGrid.viewMaximumFrequency
                const original = Math.min(
                    maximum,
                    spectra.effectOriginalPeakFrequency)
                const transformed = Math.min(
                    maximum,
                    spectra.effectTransformedPeakFrequency)
                const startX =
                    effectSpectrumGrid.xForFrequency(original)
                const endX =
                    effectSpectrumGrid.xForFrequency(transformed)
                const y = effectSpectrumGrid.plotY + 18
                const direction = endX >= startX ? 1 : -1

                ctx.strokeStyle = theme.warning
                ctx.fillStyle = theme.warning
                ctx.lineWidth = 2
                ctx.beginPath()
                ctx.arc(startX, y, 3, 0, Math.PI * 2)
                ctx.fill()
                if (Math.abs(endX - startX) > 5) {
                    ctx.beginPath()
                    ctx.moveTo(startX + direction * 5, y)
                    ctx.lineTo(endX - direction * 7, y)
                    ctx.stroke()
                    ctx.beginPath()
                    ctx.moveTo(endX, y)
                    ctx.lineTo(
                        endX - direction * 8,
                        y - 5)
                    ctx.lineTo(
                        endX - direction * 8,
                        y + 5)
                    ctx.closePath()
                    ctx.fill()
                } else {
                    ctx.beginPath()
                    ctx.arc(endX, y, 5, 0, Math.PI * 2)
                    ctx.stroke()
                }
            }

            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
        }

        Text {
            anchors.centerIn: parent
            z: 4
            visible: !spectra.sourceLoaded
                || spectra.effectOriginalSpectrum.length === 0
            text: !spectra.sourceLoaded
                ? "Spectrum appears after audio is imported"
                : (spectra.effectSpectrumAnalyzing
                    ? "Analyzing the full track…"
                    : "Full-track spectrum unavailable")
            color: theme.muted
            font.family: theme.bodyFamily
            font.pixelSize: theme.fontSize(11)
        }

        Connections {
            target: spectra

            function onEffectSpectrumChanged() {
                effectSpectrumGrid.requestPaint()
                spectrumMovementOverlay.requestPaint()
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true

        Text {
            Layout.fillWidth: true
            text: spectra.sourceLoaded
                && spectra.effectOriginalPeakFrequency > 0
                && spectra.effectTransformedPeakFrequency > 0
                ? "Tracked peak  "
                    + spectra.effectOriginalPeakFrequency
                        .toFixed(0)
                    + " Hz  →  "
                    + spectra.effectTransformedPeakFrequency
                        .toFixed(0)
                    + " Hz"
                : "Full-track average · mono mix"
            color: theme.muted
            font.family: theme.bodyFamily
            font.pixelSize: theme.fontSize(10)
            elide: Text.ElideRight
        }

        Text {
            text: spectra.effectMode === 2
                ? "Horizontal translation"
                : "Horizontal scaling"
            color: theme.accent
            font.family: theme.headingFamily
            font.pixelSize: theme.fontSize(10)
            font.weight: Font.DemiBold
        }
    }
}
