import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Spectra.Native

Item {
    id: root
    signal exportEffectRequested()

    property bool spectrumDrawerOpen: false
    property real spectrumDrawerHeight: Math.max(300, height * 0.5)

    readonly property bool compact: pageScroll.availableWidth < 980
    readonly property real spectrumDrawerMinimumHeight:
        Math.min(280, height)
    readonly property real spectrumDrawerMaximumHeight:
        Math.max(spectrumDrawerMinimumHeight, height - theme.space2)
    readonly property bool pitchMode: spectra.effectMode < 2
    readonly property real maximumShift: spectra.sourceSampleRate > 0
        ? Math.max(10, Math.min(5000, spectra.sourceSampleRate / 2))
        : 5000
    readonly property string stateLabel: spectra.effectProcessing
        ? "Processing  " + Math.round(spectra.effectProgress * 100) + "%"
        : (spectra.effectReady
            ? "Output ready"
            : (spectra.sourceLoaded ? "Ready to build" : "Import audio"))
    readonly property color stateColor: spectra.effectProcessing
        ? theme.accent
        : (spectra.effectReady
            ? theme.success
            : (spectra.sourceLoaded ? theme.warning : theme.quiet))

    Theme {
        id: theme
    }

    function signed(value, suffix, decimals) {
        const number = Number(value) || 0
        return (number > 0 ? "+" : "")
            + number.toFixed(decimals)
            + suffix
    }

    function expectedDuration() {
        if (!spectra.sourceLoaded)
            return 0
        return spectra.effectMode === 0
            ? spectra.sourceDuration / spectra.effectPitchFactor
            : spectra.sourceDuration
    }

    function modeSummary() {
        switch (spectra.effectMode) {
        case 0:
            return "Resamples the file as if tape were moving faster or slower. Pitch and duration change together."
        case 1:
            return "Uses phase-coherent overlapping FFT frames, then resamples the stretched result back to the original duration."
        default:
            return "Builds an analytic signal from overlapping FFT frames and adds the same hertz offset to every frequency."
        }
    }

    function modeCaveat() {
        switch (spectra.effectMode) {
        case 0:
            return "Clean and direct, but tempo changes with pitch."
        case 1:
            return "Keeps tempo, with the familiar softened transients of a phase vocoder."
        default:
            return "Harmonics receive unequal ratios, creating metallic or alien timbres."
        }
    }

    function clampSpectrumDrawerHeight(value) {
        return Math.max(
            spectrumDrawerMinimumHeight,
            Math.min(spectrumDrawerMaximumHeight, value))
    }

    function openSpectrumDrawer() {
        spectrumDrawerHeight =
            clampSpectrumDrawerHeight(spectrumDrawerHeight)
        spectrumDrawerOpen = true
    }

    onHeightChanged: {
        if (spectrumDrawerOpen) {
            spectrumDrawerHeight =
                clampSpectrumDrawerHeight(spectrumDrawerHeight)
        }
    }

    ScrollView {
        id: pageScroll

        anchors.fill: parent
        clip: true
        contentWidth: availableWidth
        contentHeight: pageLayout.height
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        GridLayout {
            id: pageLayout

            width: pageScroll.availableWidth
            height: Math.max(implicitHeight, pageScroll.availableHeight)
            columns: root.compact ? 1 : 2
            columnSpacing: theme.space2
            rowSpacing: theme.space2

        Panel {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredWidth: 620
            title: "Pitch & Shift"
            subtitle: "Three ways to move sound through frequency"

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: theme.space2

                SegmentedSwitch {
                    Layout.fillWidth: true
                    implicitWidth: 520
                    options: [
                        "Tape speed",
                        "Pitch shift",
                        "Frequency shift"
                    ]
                    currentIndex: spectra.effectMode
                    enabled: !spectra.effectProcessing
                    onActivated: function(index) {
                        spectra.setEffectMode(index)
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: summaryColumn.implicitHeight
                        + theme.space2 * 2
                    radius: 4
                    color: theme.canvas
                    border.width: 1
                    border.color: theme.border

                    ColumnLayout {
                        id: summaryColumn

                        anchors.fill: parent
                        anchors.margins: theme.space2
                        spacing: theme.space1

                        Text {
                            Layout.fillWidth: true
                            text: root.modeSummary()
                            color: theme.text
                            font.family: theme.bodyFamily
                            font.pixelSize: theme.fontSize(12)
                            wrapMode: Text.Wrap
                        }

                        Text {
                            Layout.fillWidth: true
                            text: root.modeCaveat()
                            color: theme.muted
                            font.family: theme.bodyFamily
                            font.pixelSize: theme.fontSize(11)
                            wrapMode: Text.Wrap
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: theme.space1
                    visible: root.pitchMode

                    RowLayout {
                        Layout.fillWidth: true

                        Text {
                            Layout.fillWidth: true
                            text: "Pitch interval"
                            color: theme.text
                            font.family: theme.headingFamily
                            font.pixelSize: theme.fontSize(13)
                            font.weight: Font.DemiBold
                        }

                        Text {
                            text: root.signed(
                                spectra.effectSemitones,
                                " semitones",
                                0)
                            color: theme.accent
                            font.family: theme.headingFamily
                            font.pixelSize: theme.fontSize(13)
                            font.weight: Font.DemiBold
                        }
                    }

                    Slider {
                        Layout.fillWidth: true
                        enabled: spectra.sourceLoaded
                            && !spectra.effectProcessing
                        from: -12
                        to: 12
                        stepSize: 1
                        snapMode: Slider.SnapAlways
                        value: spectra.effectSemitones
                        onMoved: spectra.setEffectSemitones(
                            Math.round(value))
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        columns: 5
                        columnSpacing: theme.space1

                        Repeater {
                            model: [-12, -7, 0, 7, 12]

                            AppButton {
                                required property int modelData

                                Layout.fillWidth: true
                                compact: true
                                text: modelData === 0
                                    ? "Reset"
                                    : (modelData > 0 ? "+" : "")
                                        + modelData
                                quiet: spectra.effectSemitones
                                    !== modelData
                                enabled: spectra.sourceLoaded
                                    && !spectra.effectProcessing
                                onClicked:
                                    spectra.setEffectSemitones(
                                        modelData)
                            }
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: theme.space1
                    visible: !root.pitchMode

                    RowLayout {
                        Layout.fillWidth: true

                        Text {
                            Layout.fillWidth: true
                            text: "Frequency offset"
                            color: theme.text
                            font.family: theme.headingFamily
                            font.pixelSize: theme.fontSize(13)
                            font.weight: Font.DemiBold
                        }

                        Text {
                            text: root.signed(
                                spectra.effectFrequencyShift,
                                " Hz",
                                0)
                            color: theme.accent
                            font.family: theme.headingFamily
                            font.pixelSize: theme.fontSize(13)
                            font.weight: Font.DemiBold
                        }
                    }

                    Slider {
                        Layout.fillWidth: true
                        enabled: spectra.sourceLoaded
                            && !spectra.effectProcessing
                        from: -root.maximumShift
                        to: root.maximumShift
                        stepSize: 10
                        snapMode: Slider.SnapAlways
                        value: spectra.effectFrequencyShift
                        onMoved:
                            spectra.setEffectFrequencyShift(
                                Math.round(value / 10) * 10)
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        columns: 5
                        columnSpacing: theme.space1

                        Repeater {
                            model: [-1000, -250, 0, 250, 1000]

                            AppButton {
                                required property int modelData

                                Layout.fillWidth: true
                                compact: true
                                text: modelData === 0
                                    ? "Reset"
                                    : (modelData > 0 ? "+" : "")
                                        + modelData
                                quiet:
                                    spectra.effectFrequencyShift
                                        !== modelData
                                enabled: spectra.sourceLoaded
                                    && !spectra.effectProcessing
                                    && Math.abs(modelData)
                                        <= root.maximumShift
                                onClicked:
                                    spectra.setEffectFrequencyShift(
                                        modelData)
                            }
                        }
                    }
                }

                ProgressBar {
                    Layout.fillWidth: true
                    visible: spectra.effectProcessing
                    from: 0
                    to: 1
                    value: spectra.effectProgress
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: theme.space1

                    AppButton {
                        Layout.fillWidth: true
                        text: spectra.effectProcessing
                            ? "Processing…"
                            : (spectra.effectReady
                                ? "Rebuild output"
                                : "Build output")
                        enabled: spectra.sourceLoaded
                                 && !spectra.effectProcessing
                        accentColor: theme.accent
                        onClicked: spectra.buildEffect()
                    }

                    AppButton {
                        Layout.fillWidth: true
                        text: "Export WAV"
                        quiet: true
                        enabled: spectra.effectReady
                            && !spectra.effectProcessing
                        onClicked:
                            root.exportEffectRequested()
                    }
                }

                Rectangle {
                    id: spectrumPreviewCard

                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumHeight: 250
                    radius: 4
                    color: theme.raisedPanel
                    border.width: 1
                    border.color: theme.border

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: theme.space2
                        spacing: theme.space1

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: theme.space1

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2

                                Text {
                                    Layout.fillWidth: true
                                    text: "Spectrum movement"
                                    color: theme.text
                                    font.family: theme.headingFamily
                                    font.pixelSize: theme.fontSize(13)
                                    font.weight: Font.DemiBold
                                    elide: Text.ElideRight
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: !spectra.sourceLoaded
                                        ? "Import audio to reveal its frequency shape"
                                        : (spectra.effectSpectrumAnalyzing
                                            ? "Analyzing the full track…"
                                        : (spectra.effectSpectrumPreview
                                            ? "Live prediction before building"
                                            : "Measured original and processed audio"))
                                    color: theme.muted
                                    font.family: theme.bodyFamily
                                    font.pixelSize: theme.fontSize(10)
                                    elide: Text.ElideRight
                                }
                            }

                            AppButton {
                                text: "Enlarge"
                                compact: true
                                quiet: true
                                onClicked: root.openSpectrumDrawer()
                            }
                        }

                        SpectrumMovementView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            compactMode: true
                        }
                    }
                }
            }
        }

        Panel {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredWidth: 520
            title: "Preview & compare"
            subtitle: spectra.sourceLoaded
                ? spectra.sourceFileName
                : "No source loaded"

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: theme.space2

                StatusRow {
                    Layout.fillWidth: true
                    interactive: false
                    label: "Effect state"
                    value: root.stateLabel
                    stateColor: root.stateColor
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: theme.space1
                    rowSpacing: theme.space1

                    MetricCard {
                        Layout.fillWidth: true
                        label: root.pitchMode
                            ? "Frequency rule"
                            : "Spectrum translation"
                        value: root.pitchMode
                            ? "f × "
                                + spectra.effectPitchFactor.toFixed(3)
                            : "f "
                                + root.signed(
                                    spectra.effectFrequencyShift,
                                    " Hz",
                                    0)
                        detail: spectra.effectMode === 2
                            ? "Same offset for every component"
                            : "Same ratio for every component"
                        stateColor: theme.accent
                    }

                    MetricCard {
                        Layout.fillWidth: true
                        label: "Output duration"
                        value: spectra.sourceLoaded
                            ? (spectra.effectReady
                                ? spectra.effectOutputDuration
                                : root.expectedDuration())
                                .toFixed(2) + " s"
                            : "—"
                        detail: spectra.effectMode === 0
                            ? "Changes with tape speed"
                            : "Matches the original"
                        stateColor: spectra.effectMode === 0
                            ? theme.warning
                            : theme.success
                    }

                    MetricCard {
                        Layout.fillWidth: true
                        label: "Channel layout"
                        value: spectra.effectReady
                            ? (spectra.effectOutputChannels === 1
                                ? "Mono"
                                : "Stereo")
                            : (spectra.sourceLoaded
                                ? (spectra.sourceChannels === 1
                                    ? "Mono"
                                    : (spectra.sourceChannels === 2
                                        ? "Stereo"
                                        : "Mono mix"))
                                : "—")
                        detail: "Preserved from playback source"
                        stateColor: spectra.sourceLoaded
                            ? theme.success
                            : theme.quiet
                    }

                    MetricCard {
                        Layout.fillWidth: true
                        label: "Transform"
                        value: spectra.effectMode === 0
                            ? "Resampling"
                            : spectra.effectWindowSize
                                .toLocaleString(
                                    Qt.locale(), "f", 0)
                                + " FFT"
                        detail: spectra.effectMode === 0
                            ? "Linear interpolation"
                            : spectra.effectHopSize
                                + "-sample hop"
                        stateColor: theme.accent
                    }
                }

                Text {
                    Layout.fillWidth: true
                    text: "Listen to"
                    color: theme.text
                    font.family: theme.headingFamily
                    font.pixelSize: theme.fontSize(13)
                    font.weight: Font.DemiBold
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: theme.space1

                    AppButton {
                        Layout.fillWidth: true
                        compact: true
                        text: "Original"
                        selected:
                            spectra.effectPlaybackSelection === 0
                        enabled: spectra.sourceLoaded
                        onClicked: spectra.playEffectOriginal()
                    }

                    AppButton {
                        Layout.fillWidth: true
                        compact: true
                        text: "Processed"
                        selected:
                            spectra.effectPlaybackSelection === 1
                        enabled: spectra.effectReady
                        onClicked:
                            spectra.playEffectProcessed()
                    }
                }

                TransportPlayer {
                    Layout.fillWidth: true
                    enabled: spectra.sourceLoaded
                        && (spectra.effectPlaybackSelection === 0
                            || spectra.effectReady)
                    title:
                        spectra.effectPlaybackSelection === 0
                            ? "Original full file"
                            : spectra.effectModeName
                                + " output"
                    playing: spectra.effectPlaying
                    position: spectra.playbackPosition
                    duration: spectra.playbackDuration > 0
                        ? spectra.playbackDuration
                        : (spectra.effectPlaybackSelection === 1
                            && spectra.effectReady
                            ? spectra.effectOutputDuration
                            : spectra.sourceDuration)
                    onToggleRequested:
                        spectra.toggleEffectPlayback()
                    onStopRequested: spectra.stopPlayback()
                    onSeekRequested: function(position) {
                        spectra.seekEffectPlayback(position)
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 1
                    color: theme.border
                }

                Text {
                    Layout.fillWidth: true
                    text: "How the three modes differ"
                    color: theme.text
                    font.family: theme.headingFamily
                    font.pixelSize: theme.fontSize(13)
                    font.weight: Font.DemiBold
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: theme.space1

                    StatusRow {
                        Layout.fillWidth: true
                        interactive: false
                        label: "Tape speed"
                        value: "Pitch × ratio · duration changes"
                        stateColor: spectra.effectMode === 0
                            ? theme.accent
                            : theme.quiet
                    }

                    StatusRow {
                        Layout.fillWidth: true
                        interactive: false
                        label: "Pitch shift"
                        value: "Pitch × ratio · duration fixed"
                        stateColor: spectra.effectMode === 1
                            ? theme.accent
                            : theme.quiet
                    }

                    StatusRow {
                        Layout.fillWidth: true
                        interactive: false
                        label: "Frequency shift"
                        value: "Frequency + offset · duration fixed"
                        stateColor: spectra.effectMode === 2
                            ? theme.accent
                            : theme.quiet
                    }
                }

                Item {
                    Layout.fillHeight: true
                }
            }
        }

        }
    }

    Rectangle {
        id: spectrumDrawer

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: root.spectrumDrawerOpen
            ? root.clampSpectrumDrawerHeight(
                root.spectrumDrawerHeight)
            : 0
        z: 50
        visible: height > 0.5
        enabled: root.spectrumDrawerOpen
        clip: true
        radius: 6
        color: theme.panel
        border.width: 1
        border.color: drawerResizeArea.containsMouse
            || drawerResizeArea.pressed
            ? theme.hoverBorder
            : theme.border

        Behavior on height {
            enabled: !drawerResizeArea.pressed

            NumberAnimation {
                duration: 220
                easing.type: Easing.OutCubic
            }
        }

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: 2
            color: drawerResizeArea.containsMouse
                || drawerResizeArea.pressed
                ? theme.accent
                : theme.hoverBorder
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.topMargin: 16
            spacing: 0

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 56

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: theme.space2
                    anchors.rightMargin: theme.space2
                    spacing: theme.space2

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Text {
                            Layout.fillWidth: true
                            text: "Spectrum movement"
                            color: theme.text
                            font.family: theme.headingFamily
                            font.pixelSize: theme.fontSize(15)
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                        }

                        Text {
                            Layout.fillWidth: true
                            text: !spectra.sourceLoaded
                                ? "Import audio to reveal its frequency shape"
                                : (spectra.effectSpectrumAnalyzing
                                    ? "Analyzing the full track…"
                                : (spectra.effectSpectrumPreview
                                    ? "Predicted transformation · build the output for a measured comparison"
                                    : "Measured from the original and processed audio"))
                            color: theme.muted
                            font.family: theme.bodyFamily
                            font.pixelSize: theme.fontSize(10)
                            elide: Text.ElideRight
                        }
                    }

                    AppButton {
                        text: "Collapse"
                        compact: true
                        quiet: true
                        onClicked: root.spectrumDrawerOpen = false
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: theme.border
            }

            SpectrumMovementView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.leftMargin: theme.space2
                Layout.rightMargin: theme.space2
                Layout.topMargin: theme.space1
                Layout.bottomMargin: theme.space2
                compactMode: false
            }
        }

        MouseArea {
            id: drawerResizeArea

            property real pressSceneY: 0
            property real pressHeight: 0

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: 16
            z: 5
            hoverEnabled: true
            preventStealing: true
            cursorShape: Qt.SizeVerCursor

            onPressed: function(mouse) {
                const point = mapToItem(root, mouse.x, mouse.y)
                pressSceneY = point.y
                pressHeight = spectrumDrawer.height
                root.spectrumDrawerHeight = pressHeight
            }

            onPositionChanged: function(mouse) {
                if (!pressed)
                    return
                const point = mapToItem(root, mouse.x, mouse.y)
                root.spectrumDrawerHeight =
                    root.clampSpectrumDrawerHeight(
                        pressHeight + pressSceneY - point.y)
            }

            onDoubleClicked: {
                root.spectrumDrawerHeight =
                    root.clampSpectrumDrawerHeight(
                        root.height * 0.5)
            }

            ToolTip.visible: containsMouse && !pressed
            ToolTip.text: "Drag to resize · double-click to reset"

            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                anchors.topMargin: 5
                width: 52
                height: 4
                radius: 2
                color: drawerResizeArea.containsMouse
                    || drawerResizeArea.pressed
                    ? theme.accent
                    : theme.quiet
            }
        }
    }

    Shortcut {
        sequence: "Esc"
        enabled: root.spectrumDrawerOpen
        onActivated: root.spectrumDrawerOpen = false
    }
}
