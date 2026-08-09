import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    signal exportFullFileRequested()
    signal exportSpectrogramPdfRequested()

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
    readonly property bool fixedGlobal: spectra.fullFileMode === 0
    readonly property bool energySelection:
        fixedGlobal && spectra.fullFileSelectionMode === 1
    readonly property int selectedModel:
        spectra.fullFileMode === 1
            ? 2
            : (spectra.fullFileChannelMode === 1 ? 1 : 0)
    readonly property real currentEstimatedBytes:
        !spectra.sourceLoaded
            ? 0
            : (fixedGlobal && spectra.fullFileChannelMode === 0
                ? spectra.fullFileEstimatedMonoBytes
                : spectra.fullFileEstimatedSourceBytes)
    readonly property bool buildFitsMemory:
        currentEstimatedBytes > 0
            && currentEstimatedBytes <= spectra.fullFileMemoryLimitBytes
    readonly property var reconstructionPresets:
        energySelection
            ? ["10%", "30%", "50%", "75%", "90%", "99%"]
            : (fixedGlobal
                ? ["Top 5", "1%", "10%", "30%", "50%", "All"]
                : ["5", "10", "100", "500", "1k"])

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

    function componentPresetValue(index) {
        const maximum = Math.max(1, spectra.fullFileMaximumComponents)
        if (!root.fixedGlobal) {
            const values = [5, 10, 100, 500, 1000]
            return Math.min(maximum, values[index])
        }
        switch (index) {
        case 0: return Math.min(5, maximum)
        case 1: return Math.max(1, Math.round(maximum * 0.01))
        case 2: return Math.max(1, Math.round(maximum * 0.10))
        case 3: return Math.max(1, Math.round(maximum * 0.30))
        case 4: return Math.max(1, Math.round(maximum * 0.50))
        default: return maximum
        }
    }

    function energyPresetValue(index) {
        return [0.10, 0.30, 0.50, 0.75, 0.90, 0.99][index]
    }

    function presetSelected(index) {
        if (root.energySelection) {
            return Math.abs(
                spectra.fullFileEnergyTarget
                    - root.energyPresetValue(index)) < 0.0001
        }
        return spectra.fullFileSelectedComponents
            === root.componentPresetValue(index)
    }

    function applyPreset(index) {
        if (root.energySelection)
            spectra.setFullFileEnergyTarget(
                root.energyPresetValue(index))
        else
            spectra.setFullFileComponentCount(
                root.componentPresetValue(index))
    }

    function applyExactValue() {
        const parsed = Number(exactValue.text)
        if (!Number.isFinite(parsed)) {
            exactValue.sync()
            return
        }
        if (root.energySelection)
            spectra.setFullFileEnergyTarget(parsed / 100)
        else
            spectra.setFullFileComponentCount(Math.round(parsed))
        exactValue.sync()
    }

    function selectModel(index) {
        if (index === 2)
            spectra.setFullFileMode(1)
        else
            spectra.setFullFileChannelMode(index)
    }

    function formatMemory(bytes) {
        if (bytes <= 0)
            return "—"
        const megabytes = bytes / (1024 * 1024)
        if (megabytes >= 1024)
            return (megabytes / 1024).toFixed(1) + " GB"
        return Math.round(megabytes) + " MB"
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
                            model: ["Mono FFT", "Stereo FFT", "STFT"]

                            Rectangle {
                                required property int index
                                required property string modelData

                                readonly property bool selected:
                                    root.selectedModel === index
                                readonly property bool available:
                                    index !== 1
                                        || spectra.sourceChannels > 1

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
                                opacity: spectra.sourceLoaded && available
                                    ? 1 : 0.45

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
                                    enabled: spectra.sourceLoaded
                                        && available
                                        && !spectra.fullFileProcessing
                                }

                                TapHandler {
                                    enabled: spectra.sourceLoaded
                                        && available
                                        && !spectra.fullFileProcessing
                                    onTapped: root.selectModel(index)
                                }
                            }
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: theme.space1
                        visible: root.fixedGlobal

                        Text {
                            text: "Keep by"
                            color: theme.muted
                            font.family: theme.bodyFamily
                            font.pixelSize: theme.fontSize(11)
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: theme.space1

                            Repeater {
                                model: ["FFT bins", "Spectral energy"]

                                AppButton {
                                    required property int index
                                    required property string modelData

                                    Layout.fillWidth: true
                                    text: modelData
                                    enabled: spectra.sourceLoaded
                                        && !spectra.fullFileProcessing
                                    selected:
                                        spectra.fullFileSelectionMode
                                            === index
                                    quiet:
                                        spectra.fullFileSelectionMode
                                            !== index
                                    onClicked:
                                        spectra.setFullFileSelectionMode(
                                            index)
                                }
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: theme.space1
                        visible: spectra.sourceLoaded

                        AppButton {
                            Layout.fillWidth: true
                            text: "Original"
                            selected:
                                spectra.fullFilePlaybackSelection === 0
                            quiet:
                                spectra.fullFilePlaybackSelection !== 0
                            onClicked: spectra.playFullFileOriginal()
                        }

                        AppButton {
                            Layout.fillWidth: true
                            text: "Reconstruction"
                            selected:
                                spectra.fullFilePlaybackSelection === 1
                            quiet:
                                spectra.fullFilePlaybackSelection !== 1
                            enabled: spectra.sourceLoaded
                                && spectra.fullFileMaximumComponents > 0
                                && !spectra.fullFileProcessing
                                && (spectra.fullFileReady
                                    || root.buildFitsMemory)
                            onClicked: {
                                if (spectra.fullFileReady)
                                    spectra.playFullFileReconstruction()
                                else
                                    spectra.buildFullFileModel()
                            }
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
                        title: spectra.fullFilePlaybackSelection === 0
                            ? "Original full file"
                            : (root.fixedGlobal
                                ? (spectra.fullFileChannelMode === 1
                                    ? "Stereo whole-file FFT"
                                    : "Mono whole-file FFT")
                                : "Time-varying STFT")
                        playing: spectra.fullFilePlaying
                        position: spectra.playbackPosition
                        duration: spectra.playbackDuration > 0
                            ? spectra.playbackDuration
                            : spectra.sourceDuration
                        onToggleRequested: spectra.toggleFullFilePlayback()
                        onStopRequested: spectra.stopPlayback()
                        onSeekRequested: function(position) {
                            spectra.seekFullFilePlayback(position)
                        }
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
                                text: root.energySelection
                                    ? "Retained spectral energy"
                                    : (root.fixedGlobal
                                        ? "Strongest FFT components"
                                        : "Components per frame / channel")
                                color: theme.text
                                font.family: theme.headingFamily
                                font.pixelSize: theme.fontSize(13)
                                font.weight: Font.DemiBold
                            }

                            Text {
                                Layout.fillWidth: true
                                text: root.energySelection
                                    ? (spectra.fullFileEnergyTarget * 100)
                                        .toFixed(2) + "%"
                                    : root.groupedCount(
                                        spectra.fullFileSelectedComponents)
                                        + " / "
                                        + root.groupedCount(
                                            spectra.fullFileMaximumComponents)
                                color: theme.muted
                                font.family: theme.bodyFamily
                                font.pixelSize: theme.fontSize(11)
                                horizontalAlignment: Text.AlignRight
                            }
                        }

                        Slider {
                            id: componentSlider
                            Layout.fillWidth: true
                            visible: !root.energySelection
                            enabled: spectra.sourceLoaded
                                && spectra.fullFileMaximumComponents > 0
                                && !spectra.fullFileProcessing
                            from: 1
                            to: Math.max(1, spectra.fullFileMaximumComponents)
                            stepSize: 1
                            value: Math.max(1, spectra.fullFileSelectedComponents)
                            onMoved: spectra.setFullFileComponentCount(Math.round(value))
                        }

                        Slider {
                            Layout.fillWidth: true
                            visible: root.energySelection
                            enabled: spectra.sourceLoaded
                                && !spectra.fullFileProcessing
                            from: 0.0001
                            to: 1
                            stepSize: 0.0001
                            value: spectra.fullFileEnergyTarget
                            onMoved:
                                spectra.setFullFileEnergyTarget(value)
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: theme.space1

                            TextField {
                                id: exactValue

                                Layout.fillWidth: true
                                implicitHeight: 40
                                enabled: spectra.sourceLoaded
                                    && !spectra.fullFileProcessing
                                color: theme.text
                                selectionColor: theme.accent
                                selectedTextColor: theme.text
                                font.family: theme.bodyFamily
                                font.pixelSize: theme.fontSize(11)
                                horizontalAlignment: Text.AlignRight
                                selectByMouse: true
                                inputMethodHints:
                                    Qt.ImhFormattedNumbersOnly
                                placeholderText: root.energySelection
                                    ? "Energy target"
                                    : "Component count"
                                placeholderTextColor: theme.quiet
                                leftPadding: theme.space1
                                rightPadding: theme.space1

                                function sync() {
                                    text = root.energySelection
                                        ? (spectra.fullFileEnergyTarget
                                            * 100).toFixed(2)
                                        : String(
                                            spectra.fullFileSelectedComponents)
                                }

                                background: Rectangle {
                                    radius: 4
                                    color: theme.canvas
                                    border.width: 1
                                    border.color: exactValue.activeFocus
                                        ? theme.accent
                                        : theme.border
                                }

                                onAccepted: root.applyExactValue()
                                onActiveFocusChanged: {
                                    if (!activeFocus)
                                        sync()
                                }
                                Component.onCompleted: sync()

                                Connections {
                                    target: spectra

                                    function onFullFileChanged() {
                                        if (!exactValue.activeFocus)
                                            exactValue.sync()
                                    }
                                }
                            }

                            Text {
                                text: root.energySelection
                                    ? "%"
                                    : "bins"
                                color: theme.muted
                                font.family: theme.bodyFamily
                                font.pixelSize: theme.fontSize(11)
                            }

                            AppButton {
                                text: "Apply"
                                quiet: true
                                enabled: exactValue.enabled
                                    && exactValue.text.length > 0
                                onClicked: root.applyExactValue()
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            text: root.energySelection
                                ? "The smallest ranked-bin set meeting this target is used for every output channel."
                                : (root.fixedGlobal
                                    ? "Selected from the one-sided, zero-padded whole-file transform."
                                    : "Selected independently in each overlapping frame and source channel.")
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
                                model: root.reconstructionPresets

                                AppButton {
                                    required property int index
                                    required property string modelData

                                    Layout.fillWidth: true
                                    text: modelData
                                    enabled: spectra.sourceLoaded
                                        && spectra.fullFileMaximumComponents > 0
                                        && !spectra.fullFileProcessing
                                    quiet: !root.presetSelected(index)
                                    onClicked: root.applyPreset(index)
                                }
                            }
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        visible: spectra.sourceLoaded
                        text: (root.fixedGlobal
                                ? (spectra.fullFileChannelMode === 1
                                    ? "Stereo FFT estimate "
                                    : "Mono FFT estimate ")
                                : "STFT estimate ")
                            + root.formatMemory(root.currentEstimatedBytes)
                            + "  |  Limit "
                            + root.formatMemory(
                                spectra.fullFileMemoryLimitBytes)
                        color: root.buildFitsMemory
                            ? theme.muted
                            : theme.warning
                        font.family: theme.bodyFamily
                        font.pixelSize: theme.fontSize(10)
                        wrapMode: Text.Wrap
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
                        Layout.minimumHeight: stickyBuildBar.visible
                            ? stickyBuildBar.height
                            : 0
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

                        AppButton {
                            text: "Export PDF"
                            quiet: true
                            enabled: spectra.spectrogramImageUrl.toString().length > 0
                            onClicked: root.exportSpectrogramPdfRequested()
                        }

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
                                    text: root.fixedGlobal
                                        ? (spectra.fullFileChannelMode === 1
                                            ? "Stereo FFT"
                                            : "Mono FFT")
                                        : "STFT"
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
                                    text: root.energySelection
                                        ? (spectra.fullFileEnergyTarget * 100)
                                            .toFixed(2) + "% / "
                                            + root.groupedCount(
                                                spectra.fullFileSelectedComponents)
                                        : root.groupedCount(
                                            spectra.fullFileSelectedComponents)
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
                                    text: spectra.fullFileProcessing
                                        ? "Progress"
                                        : (spectra.fullFileReady
                                            ? "Output"
                                            : "Retained energy")
                                    color: theme.muted
                                    font.family: theme.bodyFamily
                                    font.pixelSize: theme.fontSize(10.5)
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: spectra.fullFileProcessing
                                        ? Math.round(spectra.fullFileProgress * 100) + "%"
                                        : (spectra.fullFileReady
                                            ? (spectra.fullFileOutputChannels === 2
                                                ? "Stereo"
                                                : "Mono")
                                                + "  |  "
                                                + (spectra.fullFileRetainedEnergy * 100).toFixed(1)
                                                + "%"
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

    Rectangle {
        id: stickyBuildBar

        z: 10
        visible: spectra.sourceLoaded
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        width: root.compact ? root.width : inspectorPanel.width
        height: 64
        color: theme.panel
        border.width: 1
        border.color: theme.border

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: 2
            color: theme.accent
            opacity: spectra.fullFileProcessing ? 1 : 0
        }

        AppButton {
            anchors.fill: parent
            anchors.margins: theme.space1
            text: spectra.fullFileProcessing
                ? "Processing "
                    + Math.round(spectra.fullFileProgress * 100) + "%"
                : (spectra.fullFileReady
                    ? "Rebuild model"
                    : "Build model")
            enabled: spectra.fullFileMaximumComponents > 0
                && !spectra.fullFileProcessing
                && root.buildFitsMemory
            onClicked: spectra.buildFullFileModel()
        }
    }
}
