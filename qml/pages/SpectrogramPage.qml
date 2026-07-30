import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    signal importRequested()
    signal exportFullFileRequested()

    readonly property bool compact: width < 900
    readonly property string modelStateLabel: !spectra.sourceLoaded
        ? "NO AUDIO"
        : (spectra.fullFileProcessing
            ? "PROCESSING"
            : (spectra.fullFileReady ? "READY" : "NOT BUILT"))
    readonly property color modelStateColor: spectra.fullFileProcessing
        ? theme.accent
        : (spectra.fullFileReady
            ? theme.success
            : (spectra.sourceLoaded ? theme.warning : theme.quiet))

    Theme {
        id: theme
    }

    function formatDuration(seconds) {
        const safeSeconds = Math.max(0, Number(seconds) || 0)
        const totalSeconds = Math.floor(safeSeconds)
        const hours = Math.floor(totalSeconds / 3600)
        const minutes = Math.floor((totalSeconds % 3600) / 60)
        const remaining = totalSeconds % 60
        if (hours > 0)
            return hours + ":" + (minutes < 10 ? "0" : "") + minutes
                + ":" + (remaining < 10 ? "0" : "") + remaining
        return minutes + ":" + (remaining < 10 ? "0" : "") + remaining
    }

    function groupedCount(value) {
        return String(Math.max(0, Math.round(Number(value) || 0)))
            .replace(/\B(?=(\d{3})+(?!\d))/g, ",")
    }

    function presetValue(index) {
        const maximum = Math.max(1, spectra.fullFileMaximumComponents)
        switch (index) {
        case 0: return Math.min(5, maximum)
        case 1: return Math.max(1, Math.round(maximum * 0.01))
        case 2: return Math.max(1, Math.round(maximum * 0.10))
        case 3: return Math.max(1, Math.round(maximum * 0.30))
        case 4: return Math.max(1, Math.round(maximum * 0.50))
        default: return maximum
        }
    }

    ScrollView {
        id: pageScroll
        anchors.fill: parent
        clip: true
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        GridLayout {
            width: pageScroll.availableWidth
            columns: root.compact ? 1 : 2
            columnSpacing: theme.space2
            rowSpacing: theme.space2

            Panel {
                id: inspectorPanel
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: root.compact
                    ? pageScroll.availableWidth
                    : Math.max(344, Math.min(400, root.width * 0.32))
                Layout.maximumWidth: root.compact ? 100000 : 400
                Layout.minimumHeight: root.compact ? 0 : pageScroll.availableHeight
                title: spectra.sourceLoaded ? spectra.sourceFileName : "Reconstruction"
                subtitle: spectra.sourceLoaded
                    ? root.formatDuration(spectra.sourceDuration) + "  |  "
                        + (spectra.sourceSampleRate / 1000).toFixed(1) + " kHz"
                    : "Import audio to begin"

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: theme.space2

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: theme.space1

                        Text {
                            Layout.fillWidth: true
                            text: "Model"
                            color: theme.text
                            font.family: theme.headingFamily
                            font.pixelSize: theme.fontSize(13)
                            font.weight: Font.DemiBold
                        }

                        Rectangle {
                            implicitWidth: stateLabel.implicitWidth + theme.space3
                            implicitHeight: 28
                            radius: 4
                            color: Qt.rgba(
                                root.modelStateColor.r,
                                root.modelStateColor.g,
                                root.modelStateColor.b,
                                0.14)
                            border.width: 1
                            border.color: Qt.rgba(
                                root.modelStateColor.r,
                                root.modelStateColor.g,
                                root.modelStateColor.b,
                                0.42)

                            Text {
                                id: stateLabel
                                anchors.centerIn: parent
                                text: root.modelStateLabel
                                color: root.modelStateColor
                                font.family: theme.headingFamily
                                font.pixelSize: theme.fontSize(10)
                                font.weight: Font.DemiBold
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: theme.space1

                        Repeater {
                            model: ["Global FFT", "Time-varying STFT"]

                            Rectangle {
                                required property int index
                                required property string modelData

                                readonly property bool selected: spectra.fullFileMode === index

                                Layout.fillWidth: true
                                implicitHeight: 40
                                radius: 4
                                color: selected
                                    ? theme.raisedPanel
                                    : (segmentHover.hovered ? Qt.lighter(theme.canvas, 1.12) : theme.canvas)
                                border.width: 1
                                border.color: selected
                                    ? theme.accent
                                    : (segmentHover.hovered ? theme.hoverBorder : theme.border)
                                opacity: spectra.sourceLoaded ? 1 : 0.55

                                Text {
                                    anchors.centerIn: parent
                                    text: modelData
                                    color: selected ? theme.text : theme.muted
                                    font.family: theme.headingFamily
                                    font.pixelSize: theme.fontSize(11)
                                    font.weight: Font.DemiBold
                                }

                                HoverHandler {
                                    id: segmentHover
                                    enabled: spectra.sourceLoaded && !spectra.fullFileProcessing
                                }

                                TapHandler {
                                    enabled: spectra.sourceLoaded && !spectra.fullFileProcessing
                                    onTapped: spectra.setFullFileMode(index)
                                }
                            }
                        }
                    }

                    AppButton {
                        Layout.fillWidth: true
                        visible: !spectra.sourceLoaded
                        text: "Choose audio"
                        onClicked: root.importRequested()
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: theme.space1
                        visible: spectra.sourceLoaded

                        AppButton {
                            Layout.fillWidth: true
                            text: "Original"
                            onClicked: spectra.playFullFileOriginal()
                        }

                        AppButton {
                            Layout.fillWidth: true
                            text: "Reconstruction"
                            enabled: spectra.fullFileReady
                            onClicked: spectra.playFullFileReconstruction()
                        }
                    }

                    AppButton {
                        Layout.fillWidth: true
                        visible: spectra.sourceLoaded
                        enabled: spectra.fullFileReady
                        text: "Export WAV"
                        quiet: true
                        onClicked: root.exportFullFileRequested()
                    }

                    TransportPlayer {
                        Layout.fillWidth: true
                        visible: spectra.sourceLoaded
                        enabled: spectra.sourceLoaded
                        title: spectra.fullFileMode === 0
                            ? "Whole-file FFT playback"
                            : "Time-varying STFT playback"
                        playing: spectra.fullFilePlaying
                        position: spectra.playbackPosition
                        duration: spectra.playbackDuration > 0
                            ? spectra.playbackDuration
                            : spectra.sourceDuration
                        onToggleRequested: spectra.toggleFullFilePlayback()
                        onStopRequested: spectra.stopPlayback()
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 1
                        color: theme.border
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: theme.space1

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: theme.space1

                            Text {
                                Layout.fillWidth: true
                                text: spectra.fullFileMode === 0
                                    ? "Strongest FFT components"
                                    : "Components per frame"
                                color: theme.text
                                font.family: theme.headingFamily
                                font.pixelSize: theme.fontSize(13)
                                font.weight: Font.DemiBold
                            }

                            Text {
                                Layout.fillWidth: true
                                text: root.groupedCount(spectra.fullFileSelectedComponents)
                                    + " / " + root.groupedCount(spectra.fullFileMaximumComponents)
                                color: theme.muted
                                font.family: theme.bodyFamily
                                font.pixelSize: theme.fontSize(11)
                                horizontalAlignment: Text.AlignRight
                            }
                        }

                        Slider {
                            id: componentSlider
                            Layout.fillWidth: true
                            enabled: spectra.sourceLoaded
                                && spectra.fullFileMaximumComponents > 0
                                && !spectra.fullFileProcessing
                            from: 1
                            to: Math.max(1, spectra.fullFileMaximumComponents)
                            stepSize: 1
                            value: Math.max(1, spectra.fullFileSelectedComponents)
                            onMoved: spectra.setFullFileComponentCount(Math.round(value))
                        }

                        Text {
                            Layout.fillWidth: true
                            text: spectra.fullFileMode === 0
                                ? "Selected from the one-sided whole-file transform."
                                : "Selected independently in each overlapping frame."
                            color: theme.muted
                            font.family: theme.bodyFamily
                            font.pixelSize: theme.fontSize(11)
                            wrapMode: Text.Wrap
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: theme.space1

                        Text {
                            text: "Presets"
                            color: theme.text
                            font.family: theme.headingFamily
                            font.pixelSize: theme.fontSize(12)
                            font.weight: Font.DemiBold
                        }

                        GridLayout {
                            Layout.fillWidth: true
                            columns: 3
                            columnSpacing: theme.space1
                            rowSpacing: theme.space1

                            Repeater {
                                model: ["Top 5", "1%", "10%", "30%", "50%", "All"]

                                AppButton {
                                    required property int index
                                    required property string modelData

                                    Layout.fillWidth: true
                                    text: modelData
                                    enabled: spectra.sourceLoaded
                                        && spectra.fullFileMaximumComponents > 0
                                        && !spectra.fullFileProcessing
                                    quiet: spectra.fullFileSelectedComponents !== root.presetValue(index)
                                    onClicked: spectra.setFullFileComponentCount(root.presetValue(index))
                                }
                            }
                        }
                    }

                    AppButton {
                        Layout.fillWidth: true
                        text: spectra.fullFileReady ? "Rebuild model" : "Build model"
                        enabled: spectra.sourceLoaded
                            && spectra.fullFileMaximumComponents > 0
                            && !spectra.fullFileProcessing
                        onClicked: spectra.buildFullFileModel()
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: theme.space1
                        visible: spectra.fullFileProcessing

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: theme.space1

                            Text {
                                Layout.fillWidth: true
                                text: "Processing"
                                color: theme.text
                                font.family: theme.headingFamily
                                font.pixelSize: theme.fontSize(11)
                                font.weight: Font.DemiBold
                            }

                            Text {
                                text: Math.round(spectra.fullFileProgress * 100) + "%"
                                color: theme.accent
                                font.family: theme.headingFamily
                                font.pixelSize: theme.fontSize(11)
                                font.weight: Font.DemiBold
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: 8
                            radius: 4
                            color: theme.border

                            Rectangle {
                                width: parent.width * Math.max(
                                    0, Math.min(1, spectra.fullFileProgress))
                                height: parent.height
                                radius: parent.radius
                                color: theme.accent
                            }
                        }
                    }

                    Item {
                        Layout.fillHeight: true
                    }
                }
            }

            Panel {
                id: canvasPanel
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: root.compact ? 620 : pageScroll.availableHeight
                title: "Spectrogram"
                subtitle: spectra.sourceLoaded
                    ? "Imported-source STFT magnitude"
                    : "Import audio to view its time-frequency structure"

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: theme.space2

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: theme.space1

                        Item {
                            Layout.fillWidth: true
                        }

                        Text {
                            text: "-100 dB"
                            color: theme.muted
                            font.family: theme.bodyFamily
                            font.pixelSize: theme.fontSize(10)
                        }

                        Rectangle {
                            implicitWidth: 128
                            implicitHeight: 8

                            gradient: Gradient {
                                orientation: Gradient.Horizontal
                                GradientStop {
                                    position: 0
                                    color: "#11121C"
                                }
                                GradientStop {
                                    position: 1
                                    color: "#F4C048"
                                }
                            }
                        }

                        Text {
                            text: "0 dB"
                            color: theme.muted
                            font.family: theme.bodyFamily
                            font.pixelSize: theme.fontSize(10)
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: statsRow.implicitHeight + theme.space4
                        radius: 4
                        color: theme.canvas
                        border.width: 1
                        border.color: theme.border

                        RowLayout {
                            id: statsRow
                            anchors.fill: parent
                            anchors.margins: theme.space2
                            spacing: theme.space2

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: theme.space1

                                Text {
                                    text: "Model"
                                    color: theme.muted
                                    font.family: theme.bodyFamily
                                    font.pixelSize: theme.fontSize(10.5)
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: spectra.fullFileMode === 0 ? "Global FFT" : "STFT"
                                    color: theme.text
                                    font.family: theme.headingFamily
                                    font.pixelSize: theme.fontSize(13.5)
                                    font.weight: Font.DemiBold
                                    elide: Text.ElideRight
                                }
                            }

                            Rectangle {
                                implicitWidth: 1
                                Layout.fillHeight: true
                                color: theme.border
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: theme.space1

                                Text {
                                    text: "Components"
                                    color: theme.muted
                                    font.family: theme.bodyFamily
                                    font.pixelSize: theme.fontSize(10.5)
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: root.groupedCount(spectra.fullFileSelectedComponents)
                                    color: theme.text
                                    font.family: theme.headingFamily
                                    font.pixelSize: theme.fontSize(13.5)
                                    font.weight: Font.DemiBold
                                    elide: Text.ElideRight
                                }
                            }

                            Rectangle {
                                implicitWidth: 1
                                Layout.fillHeight: true
                                color: theme.border
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: theme.space1

                                Text {
                                    text: spectra.fullFileProcessing ? "Progress" : "Retained energy"
                                    color: theme.muted
                                    font.family: theme.bodyFamily
                                    font.pixelSize: theme.fontSize(10.5)
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: spectra.fullFileProcessing
                                        ? Math.round(spectra.fullFileProgress * 100) + "%"
                                        : (spectra.fullFileReady
                                            ? (spectra.fullFileRetainedEnergy * 100).toFixed(1) + "%"
                                            : "—")
                                    color: spectra.fullFileProcessing ? theme.accent : theme.text
                                    font.family: theme.headingFamily
                                    font.pixelSize: theme.fontSize(13.5)
                                    font.weight: Font.DemiBold
                                    elide: Text.ElideRight
                                }
                            }
                        }
                    }

                    Item {
                        id: plotFrame
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.minimumHeight: 320

                        Rectangle {
                            id: plotArea
                            x: 64
                            y: 8
                            width: Math.max(120, parent.width - 80)
                            height: Math.max(160, parent.height - 48)
                            color: "#11121C"

                            Image {
                                id: spectrogramImage
                                anchors.fill: parent
                                source: spectra.spectrogramImageUrl
                                fillMode: Image.Stretch
                                asynchronous: true
                                cache: false
                                smooth: true
                            }

                            Repeater {
                                model: 3

                                Rectangle {
                                    required property int index
                                    x: Math.round(parent.width * (index + 1) / 4)
                                    width: 1
                                    height: parent.height
                                    color: Qt.rgba(
                                        theme.text.r,
                                        theme.text.g,
                                        theme.text.b,
                                        0.14)
                                }
                            }

                            Repeater {
                                model: 3

                                Rectangle {
                                    required property int index
                                    y: Math.round(parent.height * (index + 1) / 4)
                                    width: parent.width
                                    height: 1
                                    color: Qt.rgba(
                                        theme.text.r,
                                        theme.text.g,
                                        theme.text.b,
                                        0.14)
                                }
                            }

                            Text {
                                anchors.centerIn: parent
                                width: parent.width - theme.space4
                                visible: spectrogramImage.status !== Image.Ready
                                text: !spectra.sourceLoaded
                                    ? "Import audio to view the spectrogram"
                                    : (spectra.fullFileProcessing
                                        ? "Building the whole-file spectrogram…"
                                        : "Spectrogram data is unavailable")
                                color: theme.muted
                                font.family: theme.headingFamily
                                font.pixelSize: theme.fontSize(14)
                                font.weight: Font.DemiBold
                                horizontalAlignment: Text.AlignHCenter
                                wrapMode: Text.Wrap
                            }

                            Rectangle {
                                anchors.fill: parent
                                color: "transparent"
                                border.width: 1
                                border.color: theme.hoverBorder
                            }
                        }

                        Text {
                            x: 0
                            y: plotArea.y
                            text: spectra.spectrogramMaximumFrequency > 0
                                ? ((spectra.spectrogramMaximumFrequency / 1000).toFixed(1) + " kHz")
                                : "— Hz"
                            color: theme.muted
                            font.family: theme.bodyFamily
                            font.pixelSize: theme.fontSize(10)
                        }

                        Text {
                            x: 24
                            y: plotArea.y + plotArea.height - implicitHeight
                            text: "0 Hz"
                            color: theme.muted
                            font.family: theme.bodyFamily
                            font.pixelSize: theme.fontSize(10)
                        }

                        Text {
                            x: plotArea.x
                            y: plotArea.y + plotArea.height + theme.space1
                            text: "0 s"
                            color: theme.muted
                            font.family: theme.bodyFamily
                            font.pixelSize: theme.fontSize(10)
                        }

                        Text {
                            x: plotArea.x + plotArea.width - implicitWidth
                            y: plotArea.y + plotArea.height + theme.space1
                            text: root.formatDuration(spectra.sourceDuration)
                            color: theme.muted
                            font.family: theme.bodyFamily
                            font.pixelSize: theme.fontSize(10)
                        }
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        columns: width < 720 ? 2 : 5
                        columnSpacing: theme.space2
                        rowSpacing: theme.space1

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: theme.space1

                            Text {
                                text: "Window"
                                color: theme.muted
                                font.family: theme.bodyFamily
                                font.pixelSize: theme.fontSize(10)
                            }
                            Text {
                                text: root.groupedCount(spectra.fullFileWindowSize)
                                color: theme.text
                                font.family: theme.headingFamily
                                font.pixelSize: theme.fontSize(11.5)
                                font.weight: Font.DemiBold
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: theme.space1

                            Text {
                                text: "Hop"
                                color: theme.muted
                                font.family: theme.bodyFamily
                                font.pixelSize: theme.fontSize(10)
                            }
                            Text {
                                text: root.groupedCount(spectra.fullFileHopSize)
                                color: theme.text
                                font.family: theme.headingFamily
                                font.pixelSize: theme.fontSize(11.5)
                                font.weight: Font.DemiBold
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: theme.space1

                            Text {
                                text: "Frames"
                                color: theme.muted
                                font.family: theme.bodyFamily
                                font.pixelSize: theme.fontSize(10)
                            }
                            Text {
                                text: root.groupedCount(spectra.fullFileFrameCount)
                                color: theme.text
                                font.family: theme.headingFamily
                                font.pixelSize: theme.fontSize(11.5)
                                font.weight: Font.DemiBold
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: theme.space1

                            Text {
                                text: "Transform"
                                color: theme.muted
                                font.family: theme.bodyFamily
                                font.pixelSize: theme.fontSize(10)
                            }
                            Text {
                                text: root.groupedCount(spectra.fullFileTransformSize)
                                color: theme.text
                                font.family: theme.headingFamily
                                font.pixelSize: theme.fontSize(11.5)
                                font.weight: Font.DemiBold
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: theme.space1

                            Text {
                                text: "Resolution"
                                color: theme.muted
                                font.family: theme.bodyFamily
                                font.pixelSize: theme.fontSize(10)
                            }
                            Text {
                                text: spectra.fullFileFrequencyResolution > 0
                                    ? spectra.fullFileFrequencyResolution.toFixed(2) + " Hz"
                                    : "—"
                                color: theme.text
                                font.family: theme.headingFamily
                                font.pixelSize: theme.fontSize(11.5)
                                font.weight: Font.DemiBold
                            }
                        }
                    }
                }
            }
        }
    }
}
