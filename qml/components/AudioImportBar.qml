import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Button {
    id: control

    property bool sourceLoaded: false
    property bool importing: false
    property string fileName: ""
    property real durationSeconds: 0
    property int sampleRate: 0
    property int channels: 0

    signal importRequested()

    readonly property bool expanded: sourceLoaded || importing

    implicitWidth: expanded ? 372 : 112
    implicitHeight: expanded ? 42 : 32
    hoverEnabled: true
    clip: true
    enabled: !importing

    Accessible.name: importing
        ? "Loading audio: " + fileName
        : (sourceLoaded
            ? "Current audio: " + fileName + ". Choose another audio file"
            : "Import audio")

    ToolTip.visible: hovered
    ToolTip.text: importing
        ? "Loading " + fileName
        : (sourceLoaded
            ? "Choose a different audio file"
            : "Import WAV, MP3, OGG, or FLAC")
    ToolTip.delay: 500

    onClicked: importRequested()

    Theme {
        id: theme
    }

    function formatDuration(secondsValue) {
        const totalSeconds = Math.max(0, Math.round(secondsValue))
        const hours = Math.floor(totalSeconds / 3600)
        const minutes = Math.floor((totalSeconds % 3600) / 60)
        const seconds = totalSeconds % 60
        const paddedSeconds = seconds < 10 ? "0" + seconds : seconds

        if (hours > 0) {
            const paddedMinutes = minutes < 10 ? "0" + minutes : minutes
            return hours + ":" + paddedMinutes + ":" + paddedSeconds
        }
        return minutes + ":" + paddedSeconds
    }

    function formatSampleRate(rate) {
        if (rate <= 0)
            return "Unknown rate"
        return (rate / 1000).toFixed(rate % 1000 === 0 ? 0 : 1) + " kHz"
    }

    function channelLabel(channelCount) {
        if (channelCount === 1)
            return "Mono"
        if (channelCount === 2)
            return "Stereo"
        return channelCount > 0 ? channelCount + " channels" : "Unknown channels"
    }

    contentItem: Item {
        clip: true

        Item {
            anchors.fill: parent
            opacity: control.expanded ? 0 : 1

            Text {
                anchors.centerIn: parent
                text: "Import audio"
                color: theme.text
                font.family: theme.headingFamily
                font.pixelSize: theme.fontSize(12)
                font.weight: Font.DemiBold
            }

            Behavior on opacity {
                NumberAnimation {
                    duration: 100
                    easing.type: Easing.OutCubic
                }
            }
        }

        Item {
            anchors.fill: parent
            opacity: control.expanded ? 1 : 0

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 10

                BusyIndicator {
                    Layout.preferredWidth: 18
                    Layout.preferredHeight: 18
                    padding: 0
                    running: control.importing
                    visible: control.importing
                }

                SpectraIcon {
                    Layout.preferredWidth: 18
                    Layout.preferredHeight: 18
                    name: "analysis"
                    color: theme.accent
                    visible: !control.importing
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 1

                    Text {
                        Layout.fillWidth: true
                        text: control.fileName
                        color: theme.text
                        font.family: theme.headingFamily
                        font.pixelSize: theme.fontSize(11.5)
                        font.weight: Font.DemiBold
                        elide: Text.ElideMiddle
                    }

                    Text {
                        Layout.fillWidth: true
                        text: control.importing
                            ? "Decoding audio…"
                            : control.formatDuration(control.durationSeconds)
                                + "  ·  "
                                + control.formatSampleRate(control.sampleRate)
                                + "  ·  "
                                + control.channelLabel(control.channels)
                        color: theme.muted
                        font.family: theme.bodyFamily
                        font.pixelSize: theme.fontSize(9.5)
                        elide: Text.ElideRight
                    }
                }

                Rectangle {
                    Layout.preferredWidth: 1
                    Layout.preferredHeight: 24
                    color: theme.border
                }

                Text {
                    Layout.preferredWidth: 64
                    Layout.fillHeight: true
                    text: control.importing ? "Loading…" : "Replace"
                    color: control.hovered ? theme.text : theme.muted
                    font.family: theme.headingFamily
                    font.pixelSize: theme.fontSize(10.5)
                    font.weight: Font.DemiBold
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    transform: Translate {
                        x: 1
                        y: -1
                    }

                    Behavior on color {
                        ColorAnimation {
                            duration: 120
                            easing.type: Easing.OutCubic
                        }
                    }
                }
            }

            Behavior on opacity {
                NumberAnimation {
                    duration: 140
                    easing.type: Easing.OutCubic
                }
            }
        }
    }

    background: Rectangle {
        radius: 4
        color: control.down
            ? Qt.lighter(theme.raisedPanel, 1.12)
            : (control.hovered ? theme.raisedPanel : theme.panel)
        border.width: 1
        border.color: control.importing || control.hovered
            ? theme.accent
            : theme.border

        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 3
            radius: 1.5
            color: theme.accent
            visible: control.expanded
        }
    }

    scale: down ? 0.98 : 1

    Behavior on implicitWidth {
        NumberAnimation {
            duration: 220
            easing.type: Easing.InOutCubic
        }
    }

    Behavior on implicitHeight {
        NumberAnimation {
            duration: 180
            easing.type: Easing.InOutCubic
        }
    }

    Behavior on scale {
        NumberAnimation {
            duration: 120
            easing.type: Easing.OutCubic
        }
    }
}
