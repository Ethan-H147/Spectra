import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root

    property bool playing: false
    property real position: 0
    property real duration: 0
    property string title: ""
    signal toggleRequested()
    signal stopRequested()

    implicitHeight: 56
    radius: 4
    color: theme.canvas
    border.width: 1
    border.color: theme.border

    Theme {
        id: theme
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: theme.space1
        spacing: theme.space1

        AppButton {
            implicitWidth: 72
            Layout.fillHeight: true
            text: root.playing ? "Pause" : "Play"
            onClicked: root.toggleRequested()
        }

        AppButton {
            implicitWidth: 64
            Layout.fillHeight: true
            text: "Stop"
            quiet: true
            onClicked: root.stopRequested()
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: theme.space1

            Text {
                Layout.fillWidth: true
                text: root.title
                color: theme.text
                font.family: theme.headingFamily
                font.pixelSize: theme.fontSize(11.5)
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 4
                radius: 2
                color: theme.border

                Rectangle {
                    width: parent.width * Math.max(0, Math.min(1, root.duration > 0 ? root.position / root.duration : 0))
                    height: parent.height
                    radius: parent.radius
                    color: theme.accent
                }
            }
        }

        Text {
            text: formatTime(root.position) + " / " + formatTime(root.duration)
            color: theme.muted
            font.family: theme.bodyFamily
            font.pixelSize: theme.fontSize(10.5)
        }
    }

    function formatTime(seconds) {
        const safeSeconds = Math.max(0, Number(seconds) || 0)
        const minutes = Math.floor(safeSeconds / 60)
        const remaining = Math.floor(safeSeconds % 60)
        return minutes + ":" + (remaining < 10 ? "0" : "") + remaining
    }
}
