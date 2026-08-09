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
    readonly property real sourceMaximumFrequency: Math.max(
        1, spectra.eqSpectrumMaximumFrequency)
    property real viewMinimumFrequency: 0
    property real viewMaximumFrequency: sourceMaximumFrequency
    property real previousSourceMaximumFrequency: 0
    readonly property bool viewIsFullRange:
        Math.abs(viewMinimumFrequency) < 0.5
        && Math.abs(viewMaximumFrequency
            - sourceMaximumFrequency) < 0.5

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

    function bandWeight(band, frequency) {
        if (!band || !band.enabled
                || band.highHz <= band.lowHz)
            return 0
        const width = band.highHz - band.lowHz
        const normalized = Math.max(
            0,
            Math.min(
                1,
                (frequency - band.lowHz) / width))
        if (band.shape === 1) {
            if (frequency <= band.lowHz
                    || frequency >= band.highHz)
                return 0
            return 0.5 - 0.5 * Math.cos(
                2 * Math.PI * normalized)
        }
        if (band.shape === 2) {
            if (frequency <= band.lowHz)
                return 1
            if (frequency >= band.highHz)
                return 0
            return 0.5 + 0.5 * Math.cos(
                Math.PI * normalized)
        }
        if (band.shape === 3) {
            if (frequency <= band.lowHz)
                return 0
            if (frequency >= band.highHz)
                return 1
            return 0.5 - 0.5 * Math.cos(
                Math.PI * normalized)
        }
        if (frequency < band.lowHz
                || frequency > band.highHz)
            return 0
        const transition = Math.max(1, width * 0.12)
        const distance = Math.min(
            frequency - band.lowHz,
            band.highHz - frequency)
        if (distance >= transition)
            return 1
        const phase = Math.max(
            0, Math.min(1, distance / transition))
        return 0.5 - 0.5 * Math.cos(Math.PI * phase)
    }

    function bandResponse(band, frequency) {
        return band.gainDb * bandWeight(band, frequency)
    }

    function bandHasResponse(band) {
        return band && band.enabled
            && Math.abs(band.gainDb) > 0.01
    }

    function bandAffectsFrequency(band, frequency) {
        if (!bandHasResponse(band))
            return false
        if (band.shape === 2)
            return frequency <= band.highHz
        if (band.shape === 3)
            return frequency >= band.lowHz
        return frequency >= band.lowHz
            && frequency <= band.highHz
    }

    function combinedResponse(bands, frequency) {
        let response = 0
        for (let index = 0; index < bands.length; ++index)
            response += bandResponse(bands[index], frequency)
        return Math.max(-24, Math.min(24, response))
    }

    function repaint() {
        grid.requestPaint()
        bandOverlay.requestPaint()
    }

    function setView(minimum, maximum) {
        const worldMaximum = sourceMaximumFrequency
        const minimumSpan = Math.min(100, worldMaximum)
        let span = Math.max(minimumSpan, maximum - minimum)
        if (span >= worldMaximum) {
            viewMinimumFrequency = 0
            viewMaximumFrequency = worldMaximum
            return
        }
        let boundedMinimum = minimum
        if (boundedMinimum < 0)
            boundedMinimum = 0
        if (boundedMinimum + span > worldMaximum)
            boundedMinimum = worldMaximum - span
        viewMinimumFrequency = boundedMinimum
        viewMaximumFrequency = boundedMinimum + span
    }

    function resetView() {
        viewMinimumFrequency = 0
        viewMaximumFrequency = sourceMaximumFrequency
    }

    function fittedMaximumFrequency() {
        const original = spectra.eqOriginalSpectrum
        const transformed = spectra.eqTransformedSpectrum
        const count = Math.max(
            original.length, transformed.length)
        if (count < 2)
            return sourceMaximumFrequency

        let peakDb = -100
        for (let index = 0; index < count; ++index) {
            if (index < original.length)
                peakDb = Math.max(peakDb, original[index])
            if (index < transformed.length)
                peakDb = Math.max(peakDb, transformed[index])
        }

        // Keep content that is visibly above the graph floor, while
        // ignoring a long tail of near-silent high-frequency bins.
        const contentFloorDb = Math.max(-84, peakDb - 54)
        let lastVisibleIndex = 0
        for (let index = 0; index < count; ++index) {
            const originalDb = index < original.length
                ? original[index] : -100
            const transformedDb = index < transformed.length
                ? transformed[index] : -100
            if (Math.max(originalDb, transformedDb)
                    >= contentFloorDb) {
                lastVisibleIndex = index
            }
        }

        let contentMaximum = lastVisibleIndex
            / (count - 1) * sourceMaximumFrequency
        const bands = spectra.eqBands
        for (let index = 0; index < bands.length; ++index)
            contentMaximum = Math.max(
                contentMaximum, bands[index].highHz)

        const minimumSpan = Math.min(
            1000, sourceMaximumFrequency)
        const padding = Math.max(120, contentMaximum * 0.12)
        return Math.min(
            sourceMaximumFrequency,
            Math.max(minimumSpan, contentMaximum + padding))
    }

    function fitContentView() {
        setView(0, fittedMaximumFrequency())
    }

    function zoomAt(frequency, scale) {
        const span = viewMaximumFrequency
            - viewMinimumFrequency
        const anchor = Math.max(
            viewMinimumFrequency,
            Math.min(viewMaximumFrequency, frequency))
        const fraction = span > 0
            ? (anchor - viewMinimumFrequency) / span
            : 0.5
        const nextSpan = span * scale
        setView(
            anchor - fraction * nextSpan,
            anchor + (1 - fraction) * nextSpan)
    }

    function panByPixels(deltaX) {
        if (grid.plotWidth <= 0)
            return
        const span = viewMaximumFrequency
            - viewMinimumFrequency
        const deltaFrequency = -deltaX
            / grid.plotWidth * span
        setView(
            viewMinimumFrequency + deltaFrequency,
            viewMaximumFrequency + deltaFrequency)
    }

    function viewLabel() {
        return frequencyLabel(viewMinimumFrequency)
            + " – " + frequencyLabel(viewMaximumFrequency)
    }

    onSelectedBandChanged: repaint()
    onViewMinimumFrequencyChanged: repaint()
    onViewMaximumFrequencyChanged: repaint()
    onSourceMaximumFrequencyChanged: {
        if (Math.abs(sourceMaximumFrequency
                - previousSourceMaximumFrequency) > 0.5) {
            previousSourceMaximumFrequency =
                sourceMaximumFrequency
            resetView()
        }
    }

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
                color: theme.warning
            }

            Text {
                text: "EQ curve"
                color: theme.text
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
                    : (root.dragMode === "pan"
                        ? "Drag horizontally to pan"
                        : "Wheel to zoom · Shift/right-drag to pan"))
            color: root.dragMode.length > 0
                ? theme.accent
                : theme.muted
            font.family: theme.bodyFamily
            font.pixelSize: theme.fontSize(10)
        }

        AppButton {
            Layout.preferredWidth: 32
            compact: true
            text: "−"
            quiet: true
            enabled: !root.viewIsFullRange
            accessibleName: "Zoom out horizontal axis"
            toolTip: "Zoom out"
            onClicked: root.zoomAt(
                root.viewMinimumFrequency, 1.6)
        }

        Text {
            Layout.preferredWidth: 126
            text: root.viewLabel()
            color: theme.text
            font.family: theme.bodyFamily
            font.pixelSize: theme.fontSize(9.5)
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
        }

        AppButton {
            Layout.preferredWidth: 32
            compact: true
            text: "+"
            quiet: true
            enabled: root.viewMaximumFrequency
                - root.viewMinimumFrequency > 100.5
            accessibleName: "Zoom in horizontal axis"
            toolTip: "Zoom in"
            onClicked: root.zoomAt(
                root.viewMinimumFrequency, 0.625)
        }

        AppButton {
            Layout.preferredWidth: 46
            compact: true
            text: "Fit"
            quiet: true
            enabled: spectra.sourceLoaded
                && spectra.eqOriginalSpectrum.length > 0
            accessibleName: "Fit spectrum content"
            toolTip: "Fit meaningful spectrum and EQ bands"
            onClicked: root.fitContentView()
        }

        AppButton {
            Layout.preferredWidth: 50
            compact: true
            text: "Full"
            quiet: true
            enabled: !root.viewIsFullRange
            accessibleName: "Show full horizontal axis"
            toolTip: "Show 0 Hz to Nyquist"
            onClicked: root.resetView()
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
            readonly property real minimumFrequency:
                root.viewMinimumFrequency
            readonly property real maximumFrequency:
                root.viewMaximumFrequency
            readonly property real frequencySpan: Math.max(
                0.0001,
                maximumFrequency - minimumFrequency)

            function xForFrequency(frequency) {
                return plotX
                    + (Math.max(
                        minimumFrequency,
                        Math.min(
                            maximumFrequency,
                            frequency))
                        - minimumFrequency)
                        / frequencySpan
                        * plotWidth
            }

            function frequencyForX(x) {
                return Math.max(
                    minimumFrequency,
                    Math.min(
                        maximumFrequency,
                        minimumFrequency
                            + (x - plotX) / plotWidth
                                * frequencySpan))
            }

            function containsPlotPoint(x, y) {
                return x >= plotX
                    && x <= plotX + plotWidth
                    && y >= plotY
                    && y <= plotY + plotHeight
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
                        minimumFrequency
                            + fraction * frequencySpan
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

                const zeroGainY = yForGain(0)
                ctx.strokeStyle = theme.hoverBorder
                ctx.setLineDash([4, 4])
                ctx.beginPath()
                ctx.moveTo(plotX, zeroGainY)
                ctx.lineTo(plotX + plotWidth, zeroGainY)
                ctx.stroke()
                ctx.setLineDash([])

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
            spectrumMaximumFrequency:
                root.sourceMaximumFrequency
            minimumFrequency: grid.minimumFrequency
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
            spectrumMaximumFrequency:
                root.sourceMaximumFrequency
            minimumFrequency: grid.minimumFrequency
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
                    const lowShelfVisible = band.shape === 2
                        && band.highHz >= grid.minimumFrequency
                    const highShelfVisible = band.shape === 3
                        && band.lowHz <= grid.maximumFrequency
                    const boundedBandVisible = band.shape !== 2
                        && band.shape !== 3
                        && band.highHz >= grid.minimumFrequency
                        && band.lowHz <= grid.maximumFrequency
                    if (!lowShelfVisible
                            && !highShelfVisible
                            && !boundedBandVisible) {
                        continue
                    }
                    const left =
                        grid.xForFrequency(band.lowHz)
                    const right =
                        grid.xForFrequency(band.highHz)
                    const selected =
                        index === root.selectedBand
                    const enabled = band.enabled
                    ctx.fillStyle = selected
                        ? Qt.rgba(0.96, 0.67, 0.20, 0.12)
                        : Qt.rgba(0.16, 0.52, 0.95,
                            enabled ? 0.07 : 0.025)
                    ctx.fillRect(
                        left,
                        grid.plotY,
                        Math.max(1, right - left),
                        grid.plotHeight)

                    if (root.bandHasResponse(band)) {
                        const responseMinimum = band.shape === 2
                            ? grid.minimumFrequency
                            : Math.max(
                                grid.minimumFrequency,
                                band.lowHz)
                        const responseMaximum = band.shape === 3
                            ? grid.maximumFrequency
                            : Math.min(
                                grid.maximumFrequency,
                                band.highHz)
                        const responseSpan = Math.max(
                            0,
                            responseMaximum - responseMinimum)
                        if (responseSpan > 0) {
                            const sampleCount = Math.max(
                                24,
                                Math.round(
                                    grid.plotWidth
                                        * responseSpan
                                        / grid.frequencySpan
                                        / 4))
                            const baselineY = grid.yForGain(0)
                            const startX = grid.xForFrequency(
                                responseMinimum)
                            const endX = grid.xForFrequency(
                                responseMaximum)
                            ctx.beginPath()
                            ctx.moveTo(startX, baselineY)
                            for (let sample = 0;
                                    sample <= sampleCount;
                                    ++sample) {
                                const fraction =
                                    sample / sampleCount
                                const frequency = responseMinimum
                                    + fraction * responseSpan
                                ctx.lineTo(
                                    grid.xForFrequency(frequency),
                                    grid.yForGain(
                                        root.bandResponse(
                                            band, frequency)))
                            }
                            ctx.lineTo(endX, baselineY)
                            ctx.closePath()
                            ctx.fillStyle = selected
                                ? Qt.rgba(
                                    0.96, 0.67, 0.20, 0.10)
                                : Qt.rgba(
                                    0.16, 0.52, 0.95, 0.06)
                            ctx.fill()

                            ctx.beginPath()
                            for (let sample = 0;
                                    sample <= sampleCount;
                                    ++sample) {
                                const fraction =
                                    sample / sampleCount
                                const frequency = responseMinimum
                                    + fraction * responseSpan
                                const x = grid.xForFrequency(
                                    frequency)
                                const y = grid.yForGain(
                                    root.bandResponse(
                                        band, frequency))
                                if (sample === 0)
                                    ctx.moveTo(x, y)
                                else
                                    ctx.lineTo(x, y)
                            }
                            ctx.strokeStyle = selected
                                ? theme.warning
                                : theme.accent
                            ctx.lineWidth = selected ? 2 : 1
                            ctx.stroke()
                        }
                    }

                    if (selected) {
                        const gainY =
                            grid.yForGain(band.gainDb)
                        const lowHandleY = grid.yForGain(
                            band.shape === 0
                                ? band.gainDb
                                : root.bandResponse(
                                    band, band.lowHz))
                        const highHandleY = grid.yForGain(
                            band.shape === 0
                                ? band.gainDb
                                : root.bandResponse(
                                    band, band.highHz))
                        ctx.fillStyle = theme.warning
                        ctx.fillRect(
                            left - 3,
                            lowHandleY - 8,
                            6,
                            16)
                        ctx.fillRect(
                            right - 3,
                            highHandleY - 8,
                            6,
                            16)
                        const label =
                            band.shapeName + "  ·  "
                            + root.frequencyLabel(band.lowHz)
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

                if (bands.length > 0) {
                    const combinedSamples = Math.max(
                        100, Math.round(grid.plotWidth / 4))
                    ctx.beginPath()
                    let drawing = false
                    for (let sample = 0;
                            sample <= combinedSamples;
                            ++sample) {
                        const fraction =
                            sample / combinedSamples
                        const frequency = grid.minimumFrequency
                            + fraction * grid.frequencySpan
                        let affected = false
                        for (let bandIndex = 0;
                                bandIndex < bands.length;
                                ++bandIndex) {
                            if (root.bandAffectsFrequency(
                                    bands[bandIndex], frequency)) {
                                affected = true
                                break
                            }
                        }
                        if (!affected) {
                            drawing = false
                            continue
                        }
                        const x = grid.plotX
                            + fraction * grid.plotWidth
                        const y = grid.yForGain(
                            root.combinedResponse(
                                bands, frequency))
                        if (!drawing)
                            ctx.moveTo(x, y)
                        else
                            ctx.lineTo(x, y)
                        drawing = true
                    }
                    ctx.strokeStyle = theme.warning
                    ctx.lineWidth = 2.4
                    ctx.stroke()
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
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            hoverEnabled: true
            cursorShape: root.dragMode === "pan"
                ? Qt.ClosedHandCursor
                : (root.dragMode === "low"
                || root.dragMode === "high"
                ? Qt.SizeHorCursor
                : (root.dragMode === "gain"
                    ? Qt.SizeVerCursor
                    : Qt.CrossCursor))

            onPressed: function(mouse) {
                root.pressX = mouse.x
                root.pressY = mouse.y
                root.dragMode = ""
                root.creatingBand = false
                if (!grid.containsPlotPoint(
                        mouse.x, mouse.y)) {
                    mouse.accepted = false
                    return
                }
                if (mouse.button === Qt.RightButton
                        || (mouse.modifiers
                            & Qt.ShiftModifier)) {
                    root.dragMode = "pan"
                    return
                }
                const bands = spectra.eqBands
                const edgeTolerance = 9
                for (let index = bands.length - 1;
                        index >= 0;
                        --index) {
                    const band = bands[index]
                    if (band.highHz < grid.minimumFrequency
                            || band.lowHz
                                > grid.maximumFrequency) {
                        continue
                    }
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
                    if (band.highHz < grid.minimumFrequency
                            || band.lowHz
                                > grid.maximumFrequency) {
                        continue
                    }
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
                if (root.dragMode === "pan") {
                    root.panByPixels(mouse.x - root.pressX)
                    root.pressX = mouse.x
                    return
                }
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

            onWheel: function(wheel) {
                if (!grid.containsPlotPoint(
                        wheel.x, wheel.y)) {
                    wheel.accepted = false
                    return
                }
                const steps = wheel.angleDelta.y / 120
                if (steps === 0) {
                    wheel.accepted = false
                    return
                }
                root.zoomAt(
                    grid.frequencyForX(wheel.x),
                    Math.pow(0.8, steps))
                wheel.accepted = true
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
        text: "Wheel to zoom · Shift/right-drag to pan · "
            + "curves overlap additively · gain is limited to ±24 dB"
        color: theme.muted
        font.family: theme.bodyFamily
        font.pixelSize: theme.fontSize(10)
        elide: Text.ElideRight
    }
}
