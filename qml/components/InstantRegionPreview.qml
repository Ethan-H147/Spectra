import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    property int previewKind: 0
    property string processedLabel: "Processed"

    readonly property bool active:
        spectra.instantPreviewKind === previewKind
    readonly property bool wideLayout: width >= 900
    readonly property string stateText: {
        if (!spectra.sourceLoaded)
            return "Import audio"
        if (!spectra.audioReady)
            return "Playback unavailable"
        if (active && spectra.instantPreviewProcessing)
            return "Updating  "
                + Math.round(
                    spectra.instantPreviewProgress * 100)
                + "%"
        if (active && spectra.instantPreviewReady)
            return "Ready"
        return "Preparing"
    }
    readonly property color stateColor:
        !spectra.sourceLoaded || !spectra.audioReady
            ? theme.quiet
            : (active && spectra.instantPreviewProcessing
                ? theme.accent
                : (active && spectra.instantPreviewReady
                    ? theme.success
                    : theme.muted))

    implicitHeight: previewContent.implicitHeight
        + theme.space2 * 2
    radius: 4
    color: theme.canvas
    border.width: 1
    border.color: theme.border

    Theme {
        id: theme
    }

    Component.onCompleted:
        spectra.requestInstantPreview(previewKind, false)

    GridLayout {
        id: previewContent

        anchors.fill: parent
        anchors.margins: theme.space2
        columns: root.wideLayout ? 2 : 1
        columnSpacing: theme.space2
        rowSpacing: theme.space1

        ColumnLayout {
            Layout.fillWidth: !root.wideLayout
            Layout.preferredWidth:
                root.wideLayout ? 560 : -1
            Layout.minimumWidth:
                root.wideLayout ? 560 : 0
            Layout.maximumWidth:
                root.wideLayout
                    ? 560
                    : Number.POSITIVE_INFINITY
            spacing: theme.space1

            RowLayout {
                Layout.fillWidth: true
                spacing: theme.space1

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Text {
                        Layout.fillWidth: true
                        text: "Live preview"
                        color: theme.text
                        font.family: theme.headingFamily
                        font.pixelSize: theme.fontSize(12)
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                    }

                    Text {
                        Layout.fillWidth: true
                        text: active
                                && spectra.instantPreviewDuration > 0
                            ? (spectra.instantPreviewDuration >= 4.95
                                ? "Loops 5 seconds  ·  "
                                : spectra.instantPreviewDuration
                                    .toFixed(1)
                                    + "-second loop  ·  ")
                                + spectra.instantPreviewRangeLabel
                            : "Loops a short section around the playhead"
                        color: theme.muted
                        font.family: theme.bodyFamily
                        font.pixelSize: theme.fontSize(10)
                        elide: Text.ElideRight
                    }
                }

                Rectangle {
                    Layout.preferredWidth: 7
                    Layout.preferredHeight: 7
                    radius: 3.5
                    color: root.stateColor
                }

                Text {
                    text: root.stateText
                    color: root.stateColor
                    font.family: theme.headingFamily
                    font.pixelSize: theme.fontSize(9.5)
                    font.weight: Font.DemiBold
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: theme.space1

                AppButton {
                    Layout.fillWidth: true
                    compact: true
                    text: "Before"
                    toolTip: "Play the original short loop"
                    selected: root.active
                        && spectra.instantPreviewPlaybackSelection === 0
                    enabled: spectra.audioReady
                        && root.active
                        && spectra.instantPreviewDuration > 0
                    onClicked:
                        spectra.playInstantPreviewOriginal()
                }

                AppButton {
                    Layout.fillWidth: true
                    compact: true
                    text: "After"
                    toolTip: "Play the "
                        + root.processedLabel
                        + " short loop"
                    selected: root.active
                        && spectra.instantPreviewPlaybackSelection === 1
                    enabled: spectra.audioReady
                        && root.active
                        && spectra.instantPreviewReady
                    onClicked:
                        spectra.playInstantPreviewProcessed()
                }

                AppButton {
                    compact: true
                    quiet: true
                    text: "Move loop here"
                    toolTip: "Center the short loop on the current playhead"
                    enabled: spectra.sourceLoaded
                        && !spectra.effectProcessing
                        && !spectra.eqProcessing
                    onClicked:
                        spectra.requestInstantPreview(
                            root.previewKind, true)
                }
            }
        }

        TransportPlayer {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            enabled: spectra.audioReady
                && root.active
                && (spectra.instantPreviewPlaybackSelection === 0
                    ? spectra.instantPreviewDuration > 0
                    : spectra.instantPreviewReady)
            title: spectra.instantPreviewPlaybackSelection === 0
                ? "Before · short loop"
                : "After · short loop"
            playing: root.active
                && spectra.instantPreviewPlaying
            position: root.active
                ? spectra.instantPreviewPlaybackPosition
                : 0
            duration: root.active
                ? spectra.instantPreviewPlaybackDuration
                : 0
            onToggleRequested:
                spectra.toggleInstantPreviewPlayback()
            onStopRequested: spectra.stopPlayback()
            onSeekRequested: function(position) {
                spectra.seekInstantPreviewPlayback(position)
            }
        }
    }
}
