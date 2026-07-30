import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    property bool playing: false
    property real position: 0
    property real duration: 0
    property string title: ""
    property bool scrubbing: false
    property real scrubPosition: 0
    readonly property real displayPosition:
        scrubbing ? scrubPosition : position

    signal toggleRequested()
    signal stopRequested()
    signal seekRequested(real position)

    implicitHeight: 56
    radius: 4
    color: theme.canvas
    border.width: 1
    border.color: theme.border

    Theme {
        id: theme
    }

    component PlayerButton: Button {
        id: playerButton

        property bool primary: false
        property bool pauseIcon: false
        property string accessibleName: ""

        implicitWidth: 36
        implicitHeight: 36
        hoverEnabled: true

        Accessible.name: accessibleName

        contentItem: Item {
            Text {
                anchors.fill: parent
                visible: !playerButton.pauseIcon
                text: playerButton.text
                color: playerButton.enabled
                    ? theme.text
                    : theme.quiet
                font.family: "Segoe UI Symbol"
                font.pixelSize: theme.fontSize(13)
                font.weight: Font.DemiBold
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }

            Row {
                anchors.centerIn: parent
                visible: playerButton.pauseIcon
                spacing: 3

                Repeater {
                    model: 2

                    Rectangle {
                        width: 3
                        height: 12
                        radius: 1
                        color: playerButton.enabled
                            ? theme.text
                            : theme.quiet
                    }
                }
            }
        }

        background: Rectangle {
            radius: width / 2
            color: {
                if (!playerButton.enabled)
                    return theme.panel
                if (playerButton.primary)
                    return playerButton.down
                        ? Qt.darker(theme.accent, 1.15)
                        : (playerButton.hovered
                            ? Qt.lighter(theme.accent, 1.12)
                            : theme.accent)
                return playerButton.down
                    ? Qt.lighter(theme.panel, 1.18)
                    : (playerButton.hovered
                        ? theme.raisedPanel
                        : theme.panel)
            }
            border.width: 1
            border.color: playerButton.hovered
                ? (playerButton.primary
                    ? Qt.lighter(theme.accent, 1.25)
                    : theme.hoverBorder)
                : (playerButton.primary
                    ? theme.accent
                    : theme.border)
            scale: playerButton.down ? 0.96 : 1

            Behavior on scale {
                NumberAnimation {
                    duration: 120
                    easing.type: Easing.OutCubic
                }
            }
        }

        ToolTip.visible: hovered
        ToolTip.text: accessibleName
        ToolTip.delay: 500
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: theme.space1
        spacing: theme.space1

        PlayerButton {
            primary: true
            text: "▶"
            pauseIcon: root.playing
            accessibleName: root.playing ? "Pause" : "Play"
            onClicked: root.toggleRequested()
        }

        PlayerButton {
            text: "■"
            accessibleName: "Stop"
            onClicked: root.stopRequested()
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4

            Text {
                Layout.fillWidth: true
                text: root.title
                color: theme.text
                font.family: theme.headingFamily
                font.pixelSize: theme.fontSize(11.5)
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }

            Item {
                Layout.fillWidth: true
                implicitHeight: 16

                Rectangle {
                    id: progressTrack

                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    height: 4
                    radius: 2
                    color: theme.border

                    Rectangle {
                        width: parent.width * root.progressRatio()
                        height: parent.height
                        radius: parent.radius
                        color: theme.accent
                    }

                    Rectangle {
                        width: 8
                        height: 8
                        radius: 4
                        x: Math.max(
                            0,
                            Math.min(
                                progressTrack.width - width,
                                progressTrack.width
                                    * root.progressRatio()
                                    - width / 2))
                        anchors.verticalCenter: parent.verticalCenter
                        color: theme.accent
                        visible: seekArea.containsMouse
                            || root.scrubbing
                    }
                }

                MouseArea {
                    id: seekArea

                    anchors.fill: parent
                    enabled: root.enabled && root.duration > 0
                    hoverEnabled: true
                    preventStealing: true
                    cursorShape: enabled
                        ? Qt.PointingHandCursor
                        : Qt.ArrowCursor

                    onPressed: function(mouse) {
                        root.scrubbing = true
                        root.updateScrubPosition(
                            mouse.x,
                            width)
                    }

                    onPositionChanged: function(mouse) {
                        if (pressed) {
                            root.updateScrubPosition(
                                mouse.x,
                                width)
                        }
                    }

                    onReleased: function(mouse) {
                        root.updateScrubPosition(
                            mouse.x,
                            width)
                        root.seekRequested(
                            root.scrubPosition)
                        root.scrubbing = false
                    }

                    onCanceled: root.scrubbing = false
                }
            }
        }

        Text {
            text: formatTime(root.displayPosition)
                + " / "
                + formatTime(root.duration)
            color: theme.muted
            font.family: theme.bodyFamily
            font.pixelSize: theme.fontSize(10.5)
        }
    }

    function progressRatio() {
        return Math.max(
            0,
            Math.min(
                1,
                duration > 0
                    ? displayPosition / duration
                    : 0))
    }

    function updateScrubPosition(localX, trackWidth) {
        const ratio = Math.max(
            0,
            Math.min(
                1,
                trackWidth > 0
                    ? localX / trackWidth
                    : 0))
        scrubPosition = ratio * duration
    }

    function formatTime(seconds) {
        const safeSeconds = Math.max(0, Number(seconds) || 0)
        const minutes = Math.floor(safeSeconds / 60)
        const remaining = Math.floor(safeSeconds % 60)
        return minutes + ":" + (remaining < 10 ? "0" : "") + remaining
    }
}
