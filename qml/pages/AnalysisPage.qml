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

                RowLayout {
                    Layout.fillWidth: true
                    spacing: theme.space2

                    Text {
                        text: "Magnitude spectrum"
                        color: theme.text
                        font.family: theme.headingFamily
                        font.pixelSize: theme.fontSize(16)
                        font.weight: Font.DemiBold
                    }

                    Text {
                        Layout.fillWidth: true
                        text: "Scroll to zoom  |  Drag to pan  |  Hover peaks for details"
                        color: theme.muted
                        font.family: theme.bodyFamily
                        font.pixelSize: theme.fontSize(10)
                        horizontalAlignment: Text.AlignRight
                        elide: Text.ElideRight
                    }
                }

                Canvas {
                    id: analysisSpectrumCanvas
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumHeight: 220

                    property real minimumFrequency: 20
                    property real maximumFrequency: dataMaximumFrequency
                    property real minimumDb: -90
                    property real maximumDb: 0
                    property bool panning: false
                    property bool hadAnalysis: false
                    property bool hoverVisible: false
                    property int hoveredPeak: -1
                    property real hoverX: 0
                    property real hoverY: 0
                    property real hoverFrequency: 0
                    property real hoverDb: 0

                    readonly property real dataMaximumFrequency: Math.max(
                        1, Math.min(20000, spectra.sourceSampleRate / 2))
                    readonly property real spectrumMaximumFrequency: Math.max(
                        1, spectra.sourceSampleRate / 2)
                    readonly property real plotX: width < 480 ? 56 : 64
                    readonly property real plotY: 8
                    readonly property real plotWidth: Math.max(48, width - plotX - 12)
                    readonly property real plotHeight: Math.max(32, height - plotY - 34)

                    function clampRange(minimum, maximum, worldMinimum, worldMaximum, minimumSpan) {
                        const worldSpan = worldMaximum - worldMinimum
                        let span = maximum - minimum
                        const boundedMinimumSpan = Math.min(minimumSpan, worldSpan)
                        if (span < boundedMinimumSpan) {
                            const center = (minimum + maximum) * 0.5
                            span = boundedMinimumSpan
                            minimum = center - span * 0.5
                            maximum = center + span * 0.5
                        }
                        if (span >= worldSpan) {
                            return [worldMinimum, worldMaximum]
                        }
                        if (minimum < worldMinimum) {
                            maximum += worldMinimum - minimum
                            minimum = worldMinimum
                        }
                        if (maximum > worldMaximum) {
                            minimum -= maximum - worldMaximum
                            maximum = worldMaximum
                        }
                        return [minimum, maximum]
                    }

                    function clampView() {
                        const frequencyRange = clampRange(
                            minimumFrequency,
                            maximumFrequency,
                            0,
                            dataMaximumFrequency,
                            Math.min(25, dataMaximumFrequency))
                        minimumFrequency = frequencyRange[0]
                        maximumFrequency = frequencyRange[1]
                        const dbRange = clampRange(
                            minimumDb, maximumDb, -120, 12, 12)
                        minimumDb = dbRange[0]
                        maximumDb = dbRange[1]
                    }

                    function resetView() {
                        minimumFrequency = dataMaximumFrequency > 20 ? 20 : 0
                        maximumFrequency = dataMaximumFrequency
                        minimumDb = -90
                        maximumDb = 0
                        clampView()
                        updateHover(spectrumPointer.mouseX, spectrumPointer.mouseY)
                        requestPaint()
                    }

                    function containsPlotPoint(x, y) {
                        return x >= plotX && x <= plotX + plotWidth
                            && y >= plotY && y <= plotY + plotHeight
                    }

                    function xForFrequency(frequency) {
                        const span = Math.max(0.0001, maximumFrequency - minimumFrequency)
                        return plotX + (frequency - minimumFrequency) / span * plotWidth
                    }

                    function yForDb(db) {
                        const span = Math.max(0.0001, maximumDb - minimumDb)
                        return plotY + (maximumDb - db) / span * plotHeight
                    }

                    function frequencyAtX(x) {
                        const position = Math.max(0, Math.min(1, (x - plotX) / plotWidth))
                        return minimumFrequency
                            + position * (maximumFrequency - minimumFrequency)
                    }

                    function dbAtY(y) {
                        const position = 1
                            - Math.max(0, Math.min(1, (y - plotY) / plotHeight))
                        return minimumDb + position * (maximumDb - minimumDb)
                    }

                    function zoomAt(x, y, wheelSteps) {
                        if (!containsPlotPoint(x, y) || wheelSteps === 0)
                            return
                        const horizontalAnchor = Math.max(
                            0, Math.min(1, (x - plotX) / plotWidth))
                        const verticalAnchor = 1 - Math.max(
                            0, Math.min(1, (y - plotY) / plotHeight))
                        const anchorFrequency = minimumFrequency
                            + horizontalAnchor * (maximumFrequency - minimumFrequency)
                        const anchorDb = minimumDb
                            + verticalAnchor * (maximumDb - minimumDb)
                        const scale = Math.pow(0.82, wheelSteps)
                        const frequencySpan =
                            (maximumFrequency - minimumFrequency) * scale
                        const dbSpan = (maximumDb - minimumDb) * scale
                        minimumFrequency =
                            anchorFrequency - horizontalAnchor * frequencySpan
                        maximumFrequency = minimumFrequency + frequencySpan
                        minimumDb = anchorDb - verticalAnchor * dbSpan
                        maximumDb = minimumDb + dbSpan
                        clampView()
                        updateHover(x, y)
                        requestPaint()
                    }

                    function panBy(deltaX, deltaY) {
                        const frequencyShift = deltaX / plotWidth
                            * (maximumFrequency - minimumFrequency)
                        const dbShift = deltaY / plotHeight
                            * (maximumDb - minimumDb)
                        minimumFrequency -= frequencyShift
                        maximumFrequency -= frequencyShift
                        minimumDb += dbShift
                        maximumDb += dbShift
                        clampView()
                        hoverVisible = false
                        requestPaint()
                    }

                    function updateHover(x, y) {
                        if (!spectra.analysisReady || !containsPlotPoint(x, y)
                                || panning) {
                            hoverVisible = false
                            hoveredPeak = -1
                            requestPaint()
                            return
                        }

                        const peaks = spectra.analysisPeaks
                        const radiusSquared = 15 * 15
                        let bestDistanceSquared = radiusSquared
                        let bestPeak = -1
                        for (let index = 0; index < peaks.length; ++index) {
                            const peak = peaks[index]
                            const frequency = Number(peak.frequency)
                            const db = Number(peak.db)
                            if (frequency < minimumFrequency
                                    || frequency > maximumFrequency
                                    || db < minimumDb || db > maximumDb) {
                                continue
                            }
                            const peakX = xForFrequency(frequency)
                            const peakY = yForDb(db)
                            const deltaX = peakX - x
                            const deltaY = peakY - y
                            const distanceSquared =
                                deltaX * deltaX + deltaY * deltaY
                            if (distanceSquared <= bestDistanceSquared) {
                                bestDistanceSquared = distanceSquared
                                bestPeak = index
                            }
                        }

                        hoveredPeak = bestPeak
                        if (bestPeak >= 0) {
                            hoverFrequency = Number(peaks[bestPeak].frequency)
                            hoverDb = Number(peaks[bestPeak].db)
                            hoverX = xForFrequency(hoverFrequency)
                            hoverY = yForDb(hoverDb)
                        } else {
                            hoverFrequency = frequencyAtX(x)
                            hoverDb = dbAtY(y)
                            hoverX = x
                            hoverY = y
                        }
                        hoverVisible = true
                        requestPaint()
                    }

                    function niceStep(span, targetTickCount) {
                        if (span <= 0 || targetTickCount <= 0)
                            return 1
                        const rawStep = span / targetTickCount
                        const magnitude = Math.pow(
                            10, Math.floor(Math.log(rawStep) / Math.LN10))
                        const normalized = rawStep / magnitude
                        let nice = 1
                        if (normalized > 5)
                            nice = 10
                        else if (normalized > 2)
                            nice = 5
                        else if (normalized > 1)
                            nice = 2
                        return nice * magnitude
                    }

                    function frequencyLabel(frequency, step) {
                        if (frequency >= 1000) {
                            const khz = frequency / 1000
                            return (step >= 1000 || khz >= 10
                                ? khz.toFixed(0) : khz.toFixed(1)) + " kHz"
                        }
                        if (step >= 100)
                            return frequency.toFixed(0) + " Hz"
                        if (step >= 10)
                            return frequency.toFixed(1) + " Hz"
                        return frequency.toFixed(2) + " Hz"
                    }

                    onPaint: {
                        const ctx = getContext("2d")
                        ctx.clearRect(0, 0, width, height)
                        ctx.fillStyle = theme.canvas
                        ctx.fillRect(0, 0, width, height)
                        ctx.strokeStyle = theme.border
                        ctx.strokeRect(0.5, 0.5, width - 1, height - 1)

                        if (!spectra.sourceLoaded || !spectra.analysisReady) {
                            const emptyMessage = spectra.sourceLoaded
                                ? "Choose a region and run analysis"
                                : "Spectrum appears after import"
                            ctx.font = theme.fontSize(11) + "px '"
                                + theme.bodyFamily + "'"
                            ctx.fillStyle = theme.muted
                            const messageWidth =
                                ctx.measureText(emptyMessage).width
                            ctx.fillText(
                                emptyMessage,
                                (width - messageWidth) * 0.5,
                                height * 0.5)
                            return
                        }

                        const horizontalTarget = Math.max(
                            2, Math.floor(plotWidth / 128))
                        const verticalTarget = Math.max(
                            2, Math.floor(plotHeight / 48))
                        const frequencyStep = niceStep(
                            maximumFrequency - minimumFrequency,
                            horizontalTarget)
                        const dbStep = niceStep(maximumDb - minimumDb, verticalTarget)
                        ctx.font = theme.fontSize(9) + "px '" + theme.bodyFamily + "'"
                        ctx.fillStyle = theme.muted
                        ctx.strokeStyle = "#35373D"
                        ctx.lineWidth = 1

                        let frequency = Math.ceil(
                            minimumFrequency / frequencyStep) * frequencyStep
                        for (let guard = 0;
                                frequency <= maximumFrequency
                                    + frequencyStep * 0.01 && guard < 64;
                                ++guard, frequency += frequencyStep) {
                            const x = xForFrequency(frequency)
                            ctx.beginPath()
                            ctx.moveTo(x, plotY)
                            ctx.lineTo(x, plotY + plotHeight)
                            ctx.stroke()
                            const label = frequencyLabel(frequency, frequencyStep)
                            const metrics = ctx.measureText(label)
                            const labelX = Math.max(
                                2,
                                Math.min(
                                    width - metrics.width - 2,
                                    x - metrics.width * 0.5))
                            ctx.fillText(label, labelX, plotY + plotHeight + 17)
                        }

                        let db = Math.ceil(minimumDb / dbStep) * dbStep
                        for (let guard = 0;
                                db <= maximumDb + dbStep * 0.01 && guard < 64;
                                ++guard, db += dbStep) {
                            const y = yForDb(db)
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

                        const values = spectra.analysisSpectrum
                        if (!values || values.length < 2)
                            return

                        ctx.save()
                        ctx.beginPath()
                        ctx.rect(plotX, plotY, plotWidth, plotHeight)
                        ctx.clip()
                        ctx.strokeStyle = theme.accent
                        ctx.lineWidth = 1.5
                        ctx.beginPath()
                        let started = false
                        for (let i = 0; i < values.length; ++i) {
                            const pointFrequency =
                                i / values.length * spectrumMaximumFrequency
                            if (pointFrequency < minimumFrequency)
                                continue
                            if (pointFrequency > maximumFrequency)
                                break
                            const x = xForFrequency(pointFrequency)
                            const y = yForDb(Number(values[i]))
                            if (!started) {
                                ctx.moveTo(x, y)
                                started = true
                            } else {
                                ctx.lineTo(x, y)
                            }
                        }
                        ctx.stroke()

                        const peaks = spectra.analysisPeaks
                        ctx.fillStyle = theme.danger
                        for (let peakIndex = 0; peakIndex < peaks.length; ++peakIndex) {
                            const peak = peaks[peakIndex]
                            const pointFrequency = Number(peak.frequency)
                            const pointDb = Number(peak.db)
                            if (pointFrequency < minimumFrequency
                                    || pointFrequency > maximumFrequency
                                    || pointDb < minimumDb || pointDb > maximumDb) {
                                continue
                            }
                            const x = xForFrequency(pointFrequency)
                            const y = yForDb(pointDb)
                            ctx.beginPath()
                            ctx.arc(x, y, 4.5, 0, Math.PI * 2)
                            ctx.fill()
                        }

                        if (hoverVisible && !panning) {
                            ctx.strokeStyle = hoveredPeak >= 0
                                ? Qt.rgba(theme.danger.r, theme.danger.g,
                                    theme.danger.b, 0.62)
                                : Qt.rgba(theme.muted.r, theme.muted.g,
                                    theme.muted.b, 0.36)
                            ctx.lineWidth = 1
                            ctx.beginPath()
                            ctx.moveTo(hoverX, plotY)
                            ctx.lineTo(hoverX, plotY + plotHeight)
                            ctx.moveTo(plotX, hoverY)
                            ctx.lineTo(plotX + plotWidth, hoverY)
                            ctx.stroke()
                            if (hoveredPeak >= 0) {
                                ctx.fillStyle = theme.danger
                                ctx.beginPath()
                                ctx.arc(hoverX, hoverY, 7, 0, Math.PI * 2)
                                ctx.fill()
                                ctx.fillStyle = theme.text
                                ctx.beginPath()
                                ctx.arc(hoverX, hoverY, 3, 0, Math.PI * 2)
                                ctx.fill()
                            }
                        }
                        ctx.restore()
                    }

                    Connections {
                        target: spectra
                        function onAnalysisChanged() {
                            if (spectra.analysisReady && !analysisSpectrumCanvas.hadAnalysis) {
                                analysisSpectrumCanvas.hadAnalysis = true
                                analysisSpectrumCanvas.resetView()
                            } else if (!spectra.analysisReady) {
                                analysisSpectrumCanvas.hadAnalysis = false
                                analysisSpectrumCanvas.resetView()
                            }
                            analysisSpectrumCanvas.requestPaint()
                        }
                    }

                    MouseArea {
                        id: spectrumPointer
                        anchors.fill: parent
                        enabled: spectra.analysisReady
                        hoverEnabled: true
                        acceptedButtons: Qt.LeftButton
                        preventStealing: true
                        cursorShape: analysisSpectrumCanvas.panning
                            ? Qt.ClosedHandCursor
                            : (analysisSpectrumCanvas.containsPlotPoint(mouseX, mouseY)
                                ? Qt.CrossCursor : Qt.ArrowCursor)

                        property real previousX: 0
                        property real previousY: 0

                        onPressed: function(mouse) {
                            if (!analysisSpectrumCanvas.containsPlotPoint(
                                    mouse.x, mouse.y)) {
                                mouse.accepted = false
                                return
                            }
                            previousX = mouse.x
                            previousY = mouse.y
                            analysisSpectrumCanvas.panning = true
                            analysisSpectrumCanvas.hoverVisible = false
                            analysisSpectrumCanvas.requestPaint()
                        }

                        onPositionChanged: function(mouse) {
                            if (analysisSpectrumCanvas.panning && pressed) {
                                const deltaX = mouse.x - previousX
                                const deltaY = mouse.y - previousY
                                previousX = mouse.x
                                previousY = mouse.y
                                analysisSpectrumCanvas.panBy(deltaX, deltaY)
                            } else {
                                analysisSpectrumCanvas.updateHover(mouse.x, mouse.y)
                            }
                        }

                        onReleased: function(mouse) {
                            analysisSpectrumCanvas.panning = false
                            analysisSpectrumCanvas.updateHover(mouse.x, mouse.y)
                        }

                        onCanceled: {
                            analysisSpectrumCanvas.panning = false
                            analysisSpectrumCanvas.hoverVisible = false
                            analysisSpectrumCanvas.requestPaint()
                        }

                        onExited: {
                            if (!analysisSpectrumCanvas.panning) {
                                analysisSpectrumCanvas.hoverVisible = false
                                analysisSpectrumCanvas.hoveredPeak = -1
                                analysisSpectrumCanvas.requestPaint()
                            }
                        }

                        onWheel: function(wheel) {
                            if (!analysisSpectrumCanvas.containsPlotPoint(
                                    wheel.x, wheel.y)) {
                                wheel.accepted = false
                                return
                            }
                            const steps = wheel.angleDelta.y / 120
                            analysisSpectrumCanvas.zoomAt(
                                wheel.x, wheel.y, steps)
                            wheel.accepted = true
                        }
                    }

                    Row {
                        anchors.top: parent.top
                        anchors.right: parent.right
                        anchors.topMargin: theme.space2
                        anchors.rightMargin: theme.space2
                        spacing: theme.space1
                        visible: spectra.analysisReady

                        SegmentedSwitch {
                            options: ["Interpolated", "FFT bins"]
                            currentIndex: spectra.analysisPeakReadoutMode
                            onActivated: function(index) {
                                spectra.setAnalysisPeakReadoutMode(index)
                            }
                        }

                        AppButton {
                            compact: true
                            quiet: true
                            text: "Reset view"
                            onClicked: analysisSpectrumCanvas.resetView()
                        }
                    }

                    Rectangle {
                        id: spectrumTooltip
                        visible: analysisSpectrumCanvas.hoverVisible
                            && !analysisSpectrumCanvas.panning
                        width: Math.max(132, tooltipColumn.implicitWidth + 24)
                        height: analysisSpectrumCanvas.hoveredPeak >= 0 ? 56 : 36
                        x: {
                            let candidate = analysisSpectrumCanvas.hoverX + 16
                            if (candidate + width > analysisSpectrumCanvas.width - 8)
                                candidate = analysisSpectrumCanvas.hoverX - width - 16
                            return Math.max(
                                8,
                                Math.min(
                                    analysisSpectrumCanvas.width - width - 8,
                                    candidate))
                        }
                        y: {
                            let candidate =
                                analysisSpectrumCanvas.hoverY - height - 16
                            if (candidate < 8)
                                candidate = analysisSpectrumCanvas.hoverY + 16
                            return Math.max(
                                8,
                                Math.min(
                                    analysisSpectrumCanvas.height - height - 8,
                                    candidate))
                        }
                        radius: 4
                        color: "#F825272D"
                        border.width: 1
                        border.color: theme.hoverBorder

                        Column {
                            id: tooltipColumn
                            anchors.left: parent.left
                            anchors.top: parent.top
                            anchors.margins: 12
                            spacing: 4

                            Text {
                                text: analysisSpectrumCanvas.hoveredPeak >= 0
                                    ? "Peak " + (analysisSpectrumCanvas.hoveredPeak + 1)
                                    : analysisSpectrumCanvas.frequencyLabel(
                                        analysisSpectrumCanvas.hoverFrequency, 1)
                                        + "   "
                                        + analysisSpectrumCanvas.hoverDb.toFixed(1)
                                        + " dB"
                                color: theme.text
                                font.family: theme.headingFamily
                                font.pixelSize: theme.fontSize(10.5)
                                font.weight: Font.DemiBold
                            }

                            Text {
                                visible: analysisSpectrumCanvas.hoveredPeak >= 0
                                text: analysisSpectrumCanvas.hoverFrequency.toFixed(2)
                                    + " Hz   "
                                    + analysisSpectrumCanvas.hoverDb.toFixed(1)
                                    + " dB"
                                color: theme.muted
                                font.family: theme.bodyFamily
                                font.pixelSize: theme.fontSize(10)
                            }
                        }
                    }

                    Component.onCompleted: {
                        hadAnalysis = spectra.analysisReady
                        resetView()
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

                        Repeater {
                            model: Math.min(
                                3, spectra.analysisPeaks.length)

                            Text {
                                required property int index

                                readonly property var peak:
                                    spectra.analysisPeaks[index]

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

                        Text {
                            visible: spectra.analysisPeakCount === 0
                            text: "No peaks above -55 dB"
                            color: theme.muted
                            font.family: theme.bodyFamily
                            font.pixelSize: theme.fontSize(11)
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
                            text: spectra.analysisPeakCount + " "
                                + (spectra.analysisPeakReadoutMode === 0
                                    ? "interpolated peaks"
                                    : "FFT-bin peaks")
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
