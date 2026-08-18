import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    signal exportEqRequested()

    component BandIconButton: Button {
        id: bandIconButton

        property string accessibleName: text

        implicitWidth: 34
        implicitHeight: 34
        padding: 0
        hoverEnabled: true
        Accessible.name: accessibleName

        contentItem: Canvas {
            property string glyph: bandIconButton.text
            property color glyphColor: bandIconButton.enabled
                ? theme.text
                : theme.quiet

            implicitWidth: 18
            implicitHeight: 18

            onPaint: {
                const ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                const centerX = width * 0.5
                const centerY = height * 0.5
                const halfSize = 5
                ctx.strokeStyle = glyphColor
                ctx.lineWidth = 1.8
                ctx.lineCap = "round"
                ctx.beginPath()
                if (glyph === "×") {
                    ctx.moveTo(
                        centerX - halfSize,
                        centerY - halfSize)
                    ctx.lineTo(
                        centerX + halfSize,
                        centerY + halfSize)
                    ctx.moveTo(
                        centerX + halfSize,
                        centerY - halfSize)
                    ctx.lineTo(
                        centerX - halfSize,
                        centerY + halfSize)
                } else {
                    ctx.moveTo(
                        centerX - halfSize, centerY)
                    ctx.lineTo(
                        centerX + halfSize, centerY)
                    if (glyph === "+") {
                        ctx.moveTo(
                            centerX, centerY - halfSize)
                        ctx.lineTo(
                            centerX, centerY + halfSize)
                    }
                }
                ctx.stroke()
            }

            onGlyphChanged: requestPaint()
            onGlyphColorChanged: requestPaint()
            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
        }

        background: Rectangle {
            radius: width / 2
            color: !bandIconButton.enabled
                ? theme.panel
                : (bandIconButton.down
                    ? Qt.lighter(theme.raisedPanel, 1.18)
                    : (bandIconButton.hovered
                        ? theme.raisedPanel
                        : theme.canvas))
            border.width: 1
            border.color: bandIconButton.hovered
                ? theme.hoverBorder
                : theme.border
            scale: bandIconButton.down ? 0.96 : 1

            Behavior on scale {
                NumberAnimation {
                    duration: 110
                    easing.type: Easing.OutCubic
                }
            }
        }

        ToolTip.visible: hovered
        ToolTip.text: accessibleName
        ToolTip.delay: 500
    }

    component PresetComboBox: ComboBox {
        id: presetControl

        property string accessibleName: "Equalizer preset"

        implicitHeight: 40
        leftPadding: 12
        rightPadding: 40
        hoverEnabled: true
        Accessible.name: accessibleName

        contentItem: Text {
            text: presetControl.displayText
            color: presetControl.enabled
                ? theme.text
                : theme.quiet
            font.family: theme.headingFamily
            font.pixelSize: theme.fontSize(11)
            font.weight: Font.DemiBold
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        indicator: Item {
            x: presetControl.width - width - 13
            y: Math.round(
                (presetControl.height - height) / 2)
            implicitWidth: 18
            implicitHeight: 18

            Canvas {
                id: presetChevron

                anchors.fill: parent
                property color strokeColor:
                    presetControl.enabled
                        ? theme.muted
                        : theme.quiet

                onPaint: {
                    const ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)
                    const centerX = width * 0.5
                    const centerY = height * 0.5
                    ctx.strokeStyle = strokeColor
                    ctx.lineWidth = 1.6
                    ctx.lineCap = "round"
                    ctx.lineJoin = "round"
                    ctx.beginPath()
                    ctx.moveTo(centerX - 5, centerY - 2.5)
                    ctx.lineTo(centerX, centerY + 2.5)
                    ctx.lineTo(centerX + 5, centerY - 2.5)
                    ctx.stroke()
                }

                onStrokeColorChanged: requestPaint()
                onWidthChanged: requestPaint()
                onHeightChanged: requestPaint()
            }
        }

        background: Rectangle {
            radius: 4
            color: presetControl.down
                ? Qt.lighter(theme.raisedPanel, 1.12)
                : (presetControl.hovered
                    ? theme.raisedPanel
                    : theme.canvas)
            border.width: 1
            border.color: presetControl.popup.visible
                ? theme.accent
                : (presetControl.hovered
                    ? theme.hoverBorder
                    : theme.border)
        }

        delegate: ItemDelegate {
            id: presetDelegate

            required property int index
            required property string modelData

            width: presetControl.width - 2
            height: 38
            highlighted:
                presetControl.highlightedIndex === index

            contentItem: Text {
                text: presetDelegate.modelData
                color: presetDelegate.index
                        === presetControl.currentIndex
                    ? theme.text
                    : theme.muted
                font.family:
                    presetDelegate.index
                            === presetControl.currentIndex
                        ? theme.headingFamily
                        : theme.bodyFamily
                font.pixelSize: theme.fontSize(10.5)
                font.weight:
                    presetDelegate.index
                            === presetControl.currentIndex
                        ? Font.DemiBold
                        : Font.Normal
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }

            background: Rectangle {
                radius: 3
                color: presetDelegate.highlighted
                    ? theme.raisedPanel
                    : "transparent"
            }
        }

        popup: Popup {
            y: presetControl.height + 4
            width: presetControl.width
            implicitHeight: Math.min(
                presetList.contentHeight + 2, 340)
            padding: 1

            contentItem: ListView {
                id: presetList

                clip: true
                implicitHeight: contentHeight
                model: presetControl.popup.visible
                    ? presetControl.delegateModel
                    : null
                currentIndex:
                    presetControl.highlightedIndex
                boundsBehavior: Flickable.StopAtBounds
                ScrollIndicator.vertical:
                    ScrollIndicator {}
            }

            background: Rectangle {
                radius: 5
                color: theme.canvas
                border.width: 1
                border.color: theme.hoverBorder
            }
        }
    }

    readonly property bool compact: pageScroll.width < 1050
    readonly property var presetDescriptions: [
        "No tonal change",
        "Subtle shelves with restrained midrange shaping",
        "A low shelf adds weight without clouding the mids",
        "Focused sub-bass lift with gentle upper-bass support",
        "Fuller lows and low mids with softened highs",
        "Reduces mud, then lifts vocal presence and air",
        "Presence bell plus a stronger high-frequency shelf",
        "A modest presence lift and smooth air shelf",
        "Bass and treble shelves for quieter listening",
        "Strongly attenuates vibration below 110 Hz",
        "A broad upper-mid cut with a softer top end",
        "Cleans lows, reduces mud, and emphasizes speech"
    ]
    readonly property string stateLabel:
        spectra.eqProcessing
            ? "Building full track  "
                + Math.round(
                    spectra.eqProgress * 100) + "%"
            : (spectra.eqReady
                ? "Full track ready"
                : (spectra.sourceLoaded
                    ? "Full track not built"
                    : "Import audio"))
    readonly property color stateColor:
        spectra.eqProcessing
            ? theme.accent
            : (spectra.eqReady
                ? theme.success
                : (spectra.sourceLoaded
                    ? theme.warning
                    : theme.quiet))

    Theme {
        id: theme
    }

    Shortcut {
        sequence: StandardKey.Delete
        enabled: eqGraph.selectedBand >= 0
            && !spectra.eqProcessing
        onActivated: {
            const removed = eqGraph.selectedBand
            spectra.removeEqBand(removed)
            eqGraph.selectedBand = Math.min(
                removed,
                spectra.eqBandCount - 1)
        }
    }

    Flickable {
        id: pageScroll

        anchors.fill: parent
        clip: true
        contentWidth: width
        contentHeight: pageLayout.height
        flickableDirection: Flickable.VerticalFlick
        boundsBehavior: Flickable.StopAtBounds
        acceptedButtons: Qt.NoButton

        ScrollBar.vertical: ScrollBar {
            id: pageScrollBar

            policy: ScrollBar.AsNeeded
        }

        ColumnLayout {
            id: pageLayout

            width: pageScroll.width - pageScrollBar.width
            height: Math.max(
                implicitHeight,
                pageScroll.height)
            spacing: theme.space2

        RowLayout {
            Layout.fillWidth: true
            spacing: theme.space2

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 3

                Text {
                    Layout.fillWidth: true
                    text: "Range EQ"
                    color: theme.text
                    font.family: theme.headingFamily
                    font.pixelSize: theme.fontSize(18)
                    font.weight: Font.DemiBold
                }

                Text {
                    Layout.fillWidth: true
                    text: "Draw a band, choose its curve, then move it vertically to shape the sound"
                    color: theme.muted
                    font.family: theme.bodyFamily
                    font.pixelSize: theme.fontSize(11)
                    elide: Text.ElideRight
                }
            }

            Rectangle {
                implicitWidth: stateText.implicitWidth
                    + theme.space2 * 2
                implicitHeight: 32
                radius: 4
                color: Qt.rgba(
                    root.stateColor.r,
                    root.stateColor.g,
                    root.stateColor.b,
                    0.12)
                border.width: 1
                border.color: root.stateColor

                Text {
                    id: stateText

                    anchors.centerIn: parent
                    text: root.stateLabel
                    color: root.stateColor
                    font.family: theme.headingFamily
                    font.pixelSize: theme.fontSize(10)
                    font.weight: Font.DemiBold
                }
            }
        }

        GridLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            columns: root.compact ? 1 : 2
            columnSpacing: theme.space2
            rowSpacing: theme.space2

            Panel {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth:
                    root.compact ? 0 : 560
                title: "Interactive spectrum"
                subtitle: spectra.sourceLoaded
                    ? "Drag empty space horizontally to add a Range band"
                    : "Import a track to begin"

                RangeEqSpectrumView {
                    id: eqGraph

                    Layout.fillWidth: true
                    Layout.fillHeight: true
                }
            }

            Panel {
                id: bandPanel

                Layout.fillWidth: root.compact
                Layout.preferredWidth:
                    root.compact ? 320 : 360
                Layout.fillHeight: true
                Layout.minimumHeight:
                    root.compact ? 320 : 0
                title: "Bands"
                subtitle: spectra.eqBandCount
                    + (spectra.eqBandCount === 1
                        ? " curve"
                        : " curves")

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: theme.space1

                    Text {
                        Layout.fillWidth: true
                        text: "Preset"
                        color: theme.text
                        font.family: theme.headingFamily
                        font.pixelSize: theme.fontSize(10.5)
                        font.weight: Font.DemiBold
                    }

                    PresetComboBox {
                        id: presetCombo

                        Layout.fillWidth: true
                        model: spectra.eqPresetNames
                        currentIndex: spectra.eqPreset
                        displayText: spectra.eqPreset >= 0
                            ? currentText
                            : "Custom"
                        enabled: spectra.sourceLoaded
                            && !spectra.eqProcessing
                        onActivated: function(index) {
                            spectra.applyEqPreset(index)
                            eqGraph.selectedBand =
                                spectra.eqBandCount > 0
                                    ? 0
                                    : -1
                            bandList.positionViewAtBeginning()
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: spectra.eqPreset >= 0
                                && spectra.eqPreset
                                    < root.presetDescriptions.length
                            ? root.presetDescriptions[
                                spectra.eqPreset]
                            : "Your editable frequency curve"
                        color: theme.muted
                        font.family: theme.bodyFamily
                        font.pixelSize: theme.fontSize(9.5)
                        elide: Text.ElideRight
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: theme.space1

                        AppButton {
                            Layout.fillWidth: true
                            text: "Add band"
                            enabled: spectra.sourceLoaded
                                && spectra.eqBandCount < 16
                                && !spectra.eqProcessing
                            accentColor: theme.accent
                            onClicked: {
                                const maximum = Math.max(
                                    1000,
                                    spectra.eqSpectrumMaximumFrequency)
                                const index = spectra.addEqBand(
                                    Math.min(200, maximum * 0.15),
                                    Math.min(1000, maximum * 0.35),
                                    0)
                                if (index >= 0)
                                    eqGraph.selectedBand = index
                            }
                        }

                        AppButton {
                            text: "Clear"
                            quiet: true
                            enabled: spectra.eqBandCount > 0
                                && !spectra.eqProcessing
                            onClicked: {
                                spectra.clearEqBands()
                                eqGraph.selectedBand = -1
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 1
                        color: theme.border
                    }

                    ListView {
                        id: bandList

                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        spacing: theme.space1
                        model: spectra.eqBands
                        boundsBehavior:
                            Flickable.StopAtBounds

                        delegate: Rectangle {
                            id: bandRow

                            required property int index
                            required property var modelData

                            width: ListView.view.width
                            height: Math.max(
                                138,
                                bandContent.implicitHeight
                                    + theme.space1 * 2)
                            radius: 4
                            color: eqGraph.selectedBand
                                    === index
                                ? theme.raisedPanel
                                : theme.canvas
                            border.width: eqGraph.selectedBand
                                    === index
                                ? 1
                                : 0
                            border.color: theme.accent
                            opacity: modelData.enabled
                                ? 1
                                : 0.58

                            MouseArea {
                                anchors.fill: parent
                                onClicked:
                                    eqGraph.selectedBand =
                                        bandRow.index
                            }

                            ColumnLayout {
                                id: bandContent

                                anchors.top: parent.top
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.topMargin: theme.space1
                                anchors.leftMargin: theme.space1
                                anchors.rightMargin: theme.space1
                                spacing: 5

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: theme.space1

                                    CheckBox {
                                        id: enabledCheck

                                        checked:
                                            bandRow.modelData.enabled
                                        enabled:
                                            !spectra.eqProcessing
                                        onToggled:
                                            spectra.setEqBandEnabled(
                                                bandRow.index,
                                                checked)
                                    }

                                    Text {
                                        Layout.fillWidth: true
                                        text: "Band "
                                            + (bandRow.index + 1)
                                            + "  ·  "
                                            + bandRow.modelData.shapeName
                                        color: theme.text
                                        font.family:
                                            theme.headingFamily
                                        font.pixelSize:
                                            theme.fontSize(11)
                                        font.weight: Font.DemiBold
                                    }

                                    Text {
                                        text: (bandRow.modelData.gainDb
                                                > 0
                                                ? "+"
                                                : "")
                                            + bandRow.modelData.gainDb
                                                .toFixed(1)
                                            + " dB"
                                        color:
                                            bandRow.modelData.gainDb
                                                > 0
                                                ? theme.success
                                                : (bandRow.modelData.gainDb
                                                    < 0
                                                    ? theme.warning
                                                    : theme.muted)
                                        font.family:
                                            theme.headingFamily
                                        font.pixelSize:
                                            theme.fontSize(11)
                                        font.weight: Font.DemiBold
                                    }

                                    BandIconButton {
                                        text: "×"
                                        accessibleName: "Delete band"
                                        enabled:
                                            !spectra.eqProcessing
                                        onClicked: {
                                            const removed =
                                                bandRow.index
                                            spectra.removeEqBand(
                                                removed)
                                            eqGraph.selectedBand =
                                                Math.min(
                                                    removed,
                                                    spectra.eqBandCount
                                                        - 1)
                                        }
                                    }
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: theme.space1

                                    PresetComboBox {
                                        Layout.preferredWidth: 124
                                        implicitHeight: 34
                                        accessibleName: "Band "
                                            + (bandRow.index + 1)
                                            + " curve shape"
                                        model:
                                            spectra.eqBandShapeNames
                                        currentIndex:
                                            bandRow.modelData.shape
                                        enabled:
                                            !spectra.eqProcessing
                                        onActivated: function(index) {
                                            spectra.setEqBandShape(
                                                bandRow.index,
                                                index)
                                        }
                                    }

                                    Text {
                                        Layout.fillWidth: true
                                        text: eqGraph.frequencyLabel(
                                                bandRow.modelData.lowHz)
                                            + " – "
                                            + eqGraph.frequencyLabel(
                                                bandRow.modelData.highHz)
                                        color: theme.muted
                                        font.family:
                                            theme.bodyFamily
                                        font.pixelSize:
                                            theme.fontSize(10)
                                        elide: Text.ElideRight
                                    }

                                    BandIconButton {
                                        text: "−"
                                        accessibleName:
                                            "Decrease gain"
                                        enabled:
                                            !spectra.eqProcessing
                                        onClicked:
                                            spectra.updateEqBand(
                                                bandRow.index,
                                                bandRow.modelData.lowHz,
                                                bandRow.modelData.highHz,
                                                bandRow.modelData.gainDb
                                                    - 0.5)
                                    }

                                    BandIconButton {
                                        text: "+"
                                        accessibleName:
                                            "Increase gain"
                                        enabled:
                                            !spectra.eqProcessing
                                        onClicked:
                                            spectra.updateEqBand(
                                                bandRow.index,
                                                bandRow.modelData.lowHz,
                                                bandRow.modelData.highHz,
                                                bandRow.modelData.gainDb
                                                    + 0.5)
                                    }
                                }
                            }
                        }

                        Text {
                            anchors.centerIn: parent
                            visible: spectra.eqBandCount === 0
                            width: parent.width
                                - theme.space2 * 2
                            text: spectra.sourceLoaded
                                ? "No bands yet\nDrag across the graph or use Add band"
                                : "Import audio before adding bands"
                            color: theme.muted
                            font.family: theme.bodyFamily
                            font.pixelSize:
                                theme.fontSize(10.5)
                            horizontalAlignment:
                                Text.AlignHCenter
                            lineHeight: 1.4
                        }
                    }
                }
            }
        }

        Panel {
            Layout.fillWidth: true
            title: "Listen & export"
            subtitle: "Preview edits live, then build the full track when you’re ready"

            ColumnLayout {
                Layout.fillWidth: true
                spacing: theme.space1

                InstantRegionPreview {
                    Layout.fillWidth: true
                    previewKind: 2
                    processedLabel: "Range EQ"
                }

                Text {
                    Layout.fillWidth: true
                    text: "Full track"
                    color: theme.text
                    font.family: theme.headingFamily
                    font.pixelSize: theme.fontSize(12)
                    font.weight: Font.DemiBold
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: theme.space1

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Text {
                            Layout.fillWidth: true
                            text: root.stateLabel
                            color: root.stateColor
                            font.family: theme.headingFamily
                            font.pixelSize: theme.fontSize(10)
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                        }

                        Text {
                            Layout.fillWidth: true
                            text: spectra.eqReady
                                ? "Listen through or export the rendered track"
                                : "Build only when you want the entire track"
                            color: theme.muted
                            font.family: theme.bodyFamily
                            font.pixelSize: theme.fontSize(10)
                            elide: Text.ElideRight
                        }
                    }

                    AppButton {
                        text: spectra.eqProcessing
                            ? "Building  "
                                + Math.round(
                                    spectra.eqProgress * 100)
                                + "%"
                            : (spectra.eqReady
                                ? "Rebuild full track"
                                : "Build full track")
                        enabled: spectra.sourceLoaded
                                 && !spectra.eqProcessing
                        accentColor: theme.accent
                        onClicked: spectra.buildEq()
                    }

                    AppButton {
                        text: "Export WAV"
                        quiet: true
                        enabled: spectra.eqReady
                            && !spectra.eqProcessing
                        onClicked: root.exportEqRequested()
                    }

                }

                ColumnLayout {
                    Layout.fillWidth: true
                    visible: spectra.eqReady
                    spacing: theme.space1

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: theme.space1

                        AppButton {
                            Layout.fillWidth: true
                            text: "Original track"
                            selected:
                                spectra.eqPlaybackSelection === 0
                            onClicked: spectra.playEqOriginal()
                        }

                        AppButton {
                            Layout.fillWidth: true
                            text: "Rendered track"
                            selected:
                                spectra.eqPlaybackSelection === 1
                            onClicked: spectra.playEqProcessed()
                        }
                    }

                    TransportPlayer {
                        Layout.fillWidth: true
                        title:
                            spectra.eqPlaybackSelection === 0
                                ? "Original full track"
                                : "Range EQ · full track"
                        playing: spectra.eqPlaying
                        position: spectra.eqPlaying
                            ? spectra.playbackPosition
                            : 0
                        duration: spectra.playbackDuration > 0
                            ? spectra.playbackDuration
                            : (spectra.eqPlaybackSelection === 1
                                ? spectra.eqOutputDuration
                                : spectra.sourceDuration)
                        onToggleRequested:
                            spectra.toggleEqPlayback()
                        onStopRequested: spectra.stopPlayback()
                        onSeekRequested: function(position) {
                            spectra.seekEqPlayback(position)
                        }
                    }
                }
            }
        }
        }
    }
}
