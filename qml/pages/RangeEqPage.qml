import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    signal importRequested()
    signal exportEqRequested()

    component BandIconButton: Button {
        id: bandIconButton

        property string accessibleName: text

        implicitWidth: 34
        implicitHeight: 34
        padding: 0
        hoverEnabled: true
        Accessible.name: accessibleName

        contentItem: Text {
            text: bandIconButton.text
            color: bandIconButton.enabled
                ? theme.text
                : theme.quiet
            font.family: "Segoe UI Symbol"
            font.pixelSize: theme.fontSize(17)
            font.weight: Font.DemiBold
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
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

        implicitHeight: 40
        leftPadding: 12
        rightPadding: 40
        hoverEnabled: true
        Accessible.name: "Equalizer preset"

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

        indicator: Text {
            x: presetControl.width - width - 13
            y: Math.round(
                (presetControl.height - height) / 2)
            text: "⌄"
            color: presetControl.enabled
                ? theme.muted
                : theme.quiet
            font.family: "Segoe UI Symbol"
            font.pixelSize: theme.fontSize(18)
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
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

    readonly property bool compact: width < 1050
    readonly property var presetDescriptions: [
        "No tonal change",
        "A gentle, general-purpose contour",
        "Adds weight below 500 Hz",
        "Focuses the lowest bass frequencies",
        "Fuller lows with slightly softer highs",
        "Reduces mud and brings voices forward",
        "Adds detail and air to high frequencies",
        "A lighter, modest high-frequency lift",
        "Lifts lows and highs for quiet listening",
        "Cuts very-low-frequency vibration and noise",
        "Softens the most fatiguing upper mids",
        "Cleans the lows and emphasizes speech"
    ]
    readonly property string stateLabel:
        spectra.eqProcessing
            ? "Processing  "
                + Math.round(
                    spectra.eqProgress * 100) + "%"
            : (spectra.eqReady
                ? "Output ready"
                : (spectra.sourceLoaded
                    ? "Ready to build"
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

    ColumnLayout {
        anchors.fill: parent
        spacing: theme.space2

        RowLayout {
            Layout.fillWidth: true
            spacing: theme.space2

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 3

                Text {
                    Layout.fillWidth: true
                    text: "Section 7 · Range EQ"
                    color: theme.text
                    font.family: theme.headingFamily
                    font.pixelSize: theme.fontSize(18)
                    font.weight: Font.DemiBold
                }

                Text {
                    Layout.fillWidth: true
                    text: "Select a frequency range, then move it vertically to shape the sound"
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

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: theme.space2

            Panel {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth: 560
                title: "Interactive spectrum"
                subtitle: spectra.sourceLoaded
                    ? "Drag empty space horizontally to add a band"
                    : "Import a track to begin"

                RangeEqSpectrumView {
                    id: eqGraph

                    Layout.fillWidth: true
                    Layout.fillHeight: true
                }
            }

            Panel {
                id: bandPanel

                Layout.preferredWidth:
                    root.compact ? 320 : 360
                Layout.fillHeight: true
                title: "Bands"
                subtitle: spectra.eqBandCount
                    + (spectra.eqBandCount === 1
                        ? " selected range"
                        : " selected ranges")

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
                                96,
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
            title: "Build & compare"
            subtitle: "The graph updates instantly; Build output measures the rendered full track"

            RowLayout {
                Layout.fillWidth: true
                spacing: theme.space2

                StatusRow {
                    Layout.preferredWidth: 260
                    interactive: false
                    label: "EQ state"
                    value: root.stateLabel
                    stateColor: root.stateColor
                }

                AppButton {
                    Layout.preferredWidth: 190
                    text: !spectra.sourceLoaded
                        ? "Import audio"
                        : (spectra.eqProcessing
                            ? "Building  "
                                + Math.round(
                                    spectra.eqProgress * 100)
                                + "%"
                            : "Build output")
                    enabled: !spectra.eqProcessing
                    accentColor: theme.accent
                    onClicked: {
                        if (spectra.sourceLoaded)
                            spectra.buildEq()
                        else
                            root.importRequested()
                    }
                }

                AppButton {
                    Layout.preferredWidth: 150
                    text: "Export WAV"
                    quiet: true
                    enabled: spectra.eqReady
                        && !spectra.eqProcessing
                    onClicked: root.exportEqRequested()
                }

                Rectangle {
                    Layout.preferredWidth: 1
                    Layout.fillHeight: true
                    color: theme.border
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: theme.space1

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: theme.space1

                        AppButton {
                            Layout.fillWidth: true
                            text: "Original"
                            quiet:
                                spectra.eqPlaybackSelection !== 0
                            enabled: spectra.sourceLoaded
                            onClicked: spectra.playEqOriginal()
                        }

                        AppButton {
                            Layout.fillWidth: true
                            text: "Processed"
                            quiet:
                                spectra.eqPlaybackSelection !== 1
                            enabled: spectra.eqReady
                            onClicked: spectra.playEqProcessed()
                        }
                    }

                    TransportPlayer {
                        Layout.fillWidth: true
                        enabled: spectra.sourceLoaded
                            && (spectra.eqPlaybackSelection === 0
                                || spectra.eqReady)
                        title:
                            spectra.eqPlaybackSelection === 0
                                ? "Original full file"
                                : "Range EQ output"
                        playing: spectra.eqPlaying
                        position: spectra.playbackPosition
                        duration: spectra.playbackDuration > 0
                            ? spectra.playbackDuration
                            : (spectra.eqPlaybackSelection === 1
                                && spectra.eqReady
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
