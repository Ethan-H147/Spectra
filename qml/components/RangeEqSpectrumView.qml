import QtQuick
import QtQuick.Layouts
import Spectra.Native

ColumnLayout {
    id: root

    property int selectedBand: -1
    property string dragMode: ""
    property real pressX: 0
    property real pressY: 0
    property real pressLowHz: 0
    property real pressHighHz: 0
    property real pressGainDb: 0
    property real creationStartHz: 0
    property real creationEndHz: 0
    property bool creatingBand: false

    spacing: theme.space1

    Theme {
        id: theme
    }

    function frequencyLabel(frequency) {
        if (frequency >= 1000)
            return (frequency / 1000).toFixed(
                frequency >= 10000 ? 0 : 1) + " kHz"
        return frequency.toFixed(0) + " Hz"
    }

    function gainLabel(gain) {
        return (gain > 0 ? "+" : "")
            + gain.toFixed(1) + " dB"
    }

    function repaint() {
        grid.requestPaint()
        bandOverlay.requestPaint()
    }

    onSelectedBandChanged: repaint()

    RowLayout {
        Layout.fillWidth: true
        spacing: theme.space2

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
                text: spectra.eqSpectrumPreview
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

        Text {
            text: root.dragMode === "gain"
                ? "Drag vertically · "
                    + root.gainLabel(
                        spectra.eqBands[root.selectedBand]
                            ? spectra.eqBands[root.selectedBand].gainDb
                            : 0)
                : (root.dragMode === "low"
                    || root.dragMode === "high"
                    ? "Drag the edge to resize"
                    : "Drag empty space to add a band")
            color: root.dragMode.length > 0
                ? theme.accent
                : theme.muted
            font.family: theme.bodyFamily
            font.pixelSize: theme.fontSize(10)
        }

        Rectangle {
            implicitWidth: spectrumState.implicitWidth
                + theme.space1 * 2
            implicitHeight: 24
            radius: 3
            visible: spectra.sourceLoaded
                && spectra.eqOriginalSpectrum.length > 0
            color: spectra.eqSpectrumPreview
                ? Qt.rgba(0.90, 0.63, 0.24, 0.14)
                : Qt.rgba(0.27, 0.75, 0.47, 0.14)
            border.width: 1
            border.color: spectra.eqSpectrumPreview
                ? theme.warning
                : theme.success

            Text {
                id: spectrumState

                anchors.centerIn: parent
                text: spectra.eqSpectrumPreview
                    ? "PREVIEW"
                    : "MEASURED"
                color: spectra.eqSpectrumPreview
                    ? theme.warning
                    : theme.success
                font.family: theme.headingFamily
                font.pixelSize: theme.fontSize(9)
                font.weight: Font.DemiBold
            }
        }
    }

    Rectangle {
        id: graph

        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumHeight: 350
        radius: 4
        color: theme.canvas
        border.width: 1
        border.color: theme.border
        clip: true

        Canvas {
            id: grid

            anchors.fill: parent

            readonly property real plotX: width < 700 ? 54 : 62
            readonly property real plotY: 16
            readonly property real plotWidth: Math.max(
                120, width - plotX - 18)
            readonly property real plotHeight: Math.max(
                80, height - plotY - 38)
            readonly property real maximumFrequency:
                Math.max(
                    1,
                    spectra.eqSpectrumMaximumFrequency)

            function xForFrequency(frequency) {
                return plotX
                    + Math.max(
                        0,
                        Math.min(
                            maximumFrequency,
                            frequency))
                        / maximumFrequency
                        * plotWidth
            }

            function frequencyForX(x) {
                return Math.max(
                    0,
                    Math.min(
                        maximumFrequency,
                        (x - plotX) / plotWidth
                            * maximumFrequency))
            }

            function yForGain(gain) {
                return plotY
                    + (24 - Math.max(
                        -24,
                        Math.min(24, gain)))
                        / 48 * plotHeight
            }

            onPaint: {
                const ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                ctx.fillStyle = theme.canvas
                ctx.fillRect(0, 0, width, height)
                ctx.font = theme.fontSize(9) + "px '"
                    + theme.bodyFamily + "'"
                ctx.lineWidth = 1

                for (let index = 0; index <= 5; ++index) {
                    const fraction = index / 5
                    const x = plotX + fraction * plotWidth
                    const frequency =
                        fraction * maximumFrequency
                    ctx.strokeStyle = "#35373D"
                    ctx.beginPath()
                    ctx.moveTo(x, plotY)
                    ctx.lineTo(x, plotY + plotHeight)
                    ctx.stroke()

                    const label =
                        root.frequencyLabel(frequency)
                    const metrics = ctx.measureText(label)
                    ctx.fillStyle = theme.muted
                    ctx.fillText(
                        label,
                        Math.max(
                            2,
                            Math.min(
                                width - metrics.width - 2,
                                x - metrics.width * 0.5)),
                        plotY + plotHeight + 19)
                }

                const dbValues = [0, -30, -60, -90]
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
                && spectra.eqOriginalSpectrum.length > 0
            spectrum: spectra.eqOriginalSpectrum
            spectrumMaximumFrequency: grid.maximumFrequency
            minimumFrequency: 0
            maximumFrequency: grid.maximumFrequency
            minimumDb: -90
            maximumDb: 0
            plotX: grid.plotX
            plotY: grid.plotY
            plotWidth: grid.plotWidth
            plotHeight: grid.plotHeight
            lineColor: theme.muted
            peakColor: theme.muted
        }

        SpectrumItem {
            anchors.fill: parent
            z: 2
            visible: spectra.sourceLoaded
                && spectra.eqTransformedSpectrum.length > 0
            spectrum: spectra.eqTransformedSpectrum
            spectrumMaximumFrequency: grid.maximumFrequency
            minimumFrequency: 0
            maximumFrequency: grid.maximumFrequency
            minimumDb: -90
            maximumDb: 0
            plotX: grid.plotX
            plotY: grid.plotY
            plotWidth: grid.plotWidth
            plotHeight: grid.plotHeight
            lineColor: theme.accent
            peakColor: theme.accent
        }

        Canvas {
            id: bandOverlay

            anchors.fill: parent
            z: 3

            onPaint: {
                const ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                const bands = spectra.eqBands
                for (let index = 0;
                        index < bands.length;
                        ++index) {
                    const band = bands[index]
                    const left =
                        grid.xForFrequency(band.lowHz)
                    const right =
                        grid.xForFrequency(band.highHz)
                    const selected =
                        index === root.selectedBand
                    const enabled = band.enabled
                    ctx.fillStyle = selected
                        ? Qt.rgba(0.96, 0.67, 0.20, 0.18)
                        : Qt.rgba(0.16, 0.52, 0.95,
                            enabled ? 0.10 : 0.04)
                    ctx.fillRect(
                        left,
                        grid.plotY,
                        Math.max(1, right - left),
                        grid.plotHeight)
                    ctx.strokeStyle = selected
                        ? theme.warning
                        : (enabled
                            ? theme.accent
                            : theme.quiet)
                    ctx.lineWidth = selected ? 2 : 1
                    ctx.strokeRect(
                        left + 0.5,
                        grid.plotY + 0.5,
                        Math.max(1, right - left - 1),
                        grid.plotHeight - 1)

                    const gainY =
                        grid.yForGain(band.gainDb)
                    ctx.beginPath()
                    ctx.moveTo(left, gainY)
                    ctx.lineTo(right, gainY)
                    ctx.stroke()

                    if (selected) {
                        ctx.fillStyle = theme.warning
                        ctx.fillRect(
                            left - 3,
                            gainY - 8,
                            6,
                            16)
                        ctx.fillRect(
                            right - 3,
                            gainY - 8,
                            6,
                            16)
                        const label =
                            root.frequencyLabel(band.lowHz)
                            + " – "
                            + root.frequencyLabel(band.highHz)
                            + "  ·  "
                            + root.gainLabel(band.gainDb)
                        ctx.font = theme.fontSize(10)
                            + "px '" + theme.bodyFamily + "'"
                        const metrics =
                            ctx.measureText(label)
                        const labelX = Math.max(
                            grid.plotX + 4,
                            Math.min(
                                grid.plotX
                                    + grid.plotWidth
                                    - metrics.width - 12,
                                left + 8))
                        const labelY = Math.max(
                            grid.plotY + 17,
                            gainY - 9)
                        ctx.fillStyle =
                            Qt.rgba(0.08, 0.09, 0.11, 0.90)
                        ctx.fillRect(
                            labelX - 5,
                            labelY - 13,
                            metrics.width + 10,
                            19)
                        ctx.fillStyle = theme.text
                        ctx.fillText(
                            label, labelX, labelY)
                    }
                }

                if (root.creatingBand) {
                    const left = grid.xForFrequency(
                        Math.min(
                            root.creationStartHz,
                            root.creationEndHz))
                    const right = grid.xForFrequency(
                        Math.max(
                            root.creationStartHz,
                            root.creationEndHz))
                    ctx.fillStyle =
                        Qt.rgba(0.96, 0.67, 0.20, 0.16)
                    ctx.fillRect(
                        left,
                        grid.plotY,
                        right - left,
                        grid.plotHeight)
                    ctx.strokeStyle = theme.warning
                    ctx.lineWidth = 2
                    ctx.setLineDash([5, 4])
                    ctx.strokeRect(
                        left + 0.5,
                        grid.plotY + 0.5,
                        Math.max(1, right - left - 1),
                        grid.plotHeight - 1)
                    ctx.setLineDash([])
                }
            }

            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
        }

        MouseArea {
            id: interaction

            anchors.fill: parent
            z: 4
            enabled: spectra.sourceLoaded
                && spectra.eqOriginalSpectrum.length > 0
                && !spectra.eqProcessing
            acceptedButtons: Qt.LeftButton
            cursorShape: root.dragMode === "low"
                || root.dragMode === "high"
                ? Qt.SizeHorCursor
                : (root.dragMode === "gain"
                    ? Qt.SizeVerCursor
                    : Qt.CrossCursor)

            onPressed: function(mouse) {
                root.pressX = mouse.x
                root.pressY = mouse.y
                root.dragMode = ""
                root.creatingBand = false
                const bands = spectra.eqBands
                const edgeTolerance = 9
                for (let index = bands.length - 1;
                        index >= 0;
                        --index) {
                    const band = bands[index]
                    const left =
                        grid.xForFrequency(band.lowHz)
                    const right =
                        grid.xForFrequency(band.highHz)
                    if (Math.abs(mouse.x - left)
                            <= edgeTolerance) {
                        root.selectedBand = index
                        root.dragMode = "low"
                        root.pressLowHz = band.lowHz
                        root.pressHighHz = band.highHz
                        root.pressGainDb = band.gainDb
                        return
                    }
                    if (Math.abs(mouse.x - right)
                            <= edgeTolerance) {
                        root.selectedBand = index
                        root.dragMode = "high"
                        root.pressLowHz = band.lowHz
                        root.pressHighHz = band.highHz
                        root.pressGainDb = band.gainDb
                        return
                    }
                }
                for (let index = bands.length - 1;
                        index >= 0;
                        --index) {
                    const band = bands[index]
                    if (mouse.x >=
                            grid.xForFrequency(band.lowHz)
                            && mouse.x <=
                            grid.xForFrequency(
                                band.highHz)) {
                        root.selectedBand = index
                        root.dragMode = "gain"
                        root.pressLowHz = band.lowHz
                        root.pressHighHz = band.highHz
                        root.pressGainDb = band.gainDb
                        return
                    }
                }

                root.selectedBand = -1
                root.dragMode = "create"
                root.creationStartHz =
                    grid.frequencyForX(mouse.x)
                root.creationEndHz =
                    root.creationStartHz
            }

            onPositionChanged: function(mouse) {
                if (!pressed)
                    return
                if (root.dragMode === "create") {
                    root.creationEndHz =
                        grid.frequencyForX(mouse.x)
                    root.creatingBand =
                        Math.abs(mouse.x - root.pressX) >= 6
                    bandOverlay.requestPaint()
                    return
                }
                if (root.selectedBand < 0)
                    return

                if (root.dragMode === "gain") {
                    const delta =
                        (root.pressY - mouse.y)
                        / grid.plotHeight * 48
                    const gain = Math.max(
                        -24,
                        Math.min(
                            24,
                            Math.round(
                                (root.pressGainDb + delta)
                                    * 2) / 2))
                    spectra.updateEqBand(
                        root.selectedBand,
                        root.pressLowHz,
                        root.pressHighHz,
                        gain)
                } else {
                    const frequency =
                        grid.frequencyForX(mouse.x)
                    const minimumWidth = 20
                    if (root.dragMode === "low") {
                        spectra.updateEqBand(
                            root.selectedBand,
                            Math.min(
                                frequency,
                                root.pressHighHz
                                    - minimumWidth),
                            root.pressHighHz,
                            root.pressGainDb)
                    } else if (root.dragMode === "high") {
                        spectra.updateEqBand(
                            root.selectedBand,
                            root.pressLowHz,
                            Math.max(
                                frequency,
                                root.pressLowHz
                                    + minimumWidth),
                            root.pressGainDb)
                    }
                }
            }

            onReleased: function(mouse) {
                if (root.dragMode === "create"
                        && root.creatingBand) {
                    const index = spectra.addEqBand(
                        Math.min(
                            root.creationStartHz,
                            root.creationEndHz),
                        Math.max(
                            root.creationStartHz,
                            root.creationEndHz),
                        0)
                    if (index >= 0)
                        root.selectedBand = index
                }
                root.dragMode = ""
                root.creatingBand = false
                root.repaint()
            }

            onCanceled: {
                root.dragMode = ""
                root.creatingBand = false
                root.repaint()
            }
        }

        Text {
            anchors.centerIn: parent
            z: 5
            visible: !spectra.sourceLoaded
                || spectra.eqOriginalSpectrum.length === 0
            text: !spectra.sourceLoaded
                ? "Import audio to start shaping its spectrum"
                : (spectra.eqSpectrumAnalyzing
                    ? "Analyzing the full track…"
                    : "Full-track spectrum unavailable")
            color: theme.muted
            font.family: theme.bodyFamily
            font.pixelSize: theme.fontSize(11)
        }

        Connections {
            target: spectra

            function onEqChanged() {
                if (root.selectedBand >=
                        spectra.eqBandCount) {
                    root.selectedBand =
                        spectra.eqBandCount - 1
                }
                root.repaint()
            }

            function onEqSpectrumChanged() {
                root.repaint()
            }
        }
    }

    Text {
        Layout.fillWidth: true
        text: "Bands overlap additively · smooth 12% transitions · "
            + "gain is limited to ±24 dB per band"
        color: theme.muted
        font.family: theme.bodyFamily
        font.pixelSize: theme.fontSize(10)
        elide: Text.ElideRight
    }
}
