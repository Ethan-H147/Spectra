import QtQuick

Rectangle {
    id: control

    property var options: []
    property int currentIndex: 0

    signal activated(int index)

    implicitWidth: 184
    implicitHeight: 32
    radius: height / 2
    color: theme.canvas
    border.width: 1
    border.color: theme.border
    clip: true

    readonly property real inset: 2
    readonly property real segmentWidth:
        (width - inset * 2) / Math.max(1, options.length)

    Theme {
        id: theme
    }

    Rectangle {
        x: control.inset
            + control.currentIndex * control.segmentWidth
        y: control.inset
        width: control.segmentWidth
        height: control.height - control.inset * 2
        radius: height / 2
        color: theme.raisedPanel
        border.width: 1
        border.color: theme.hoverBorder

        Behavior on x {
            NumberAnimation {
                duration: 180
                easing.type: Easing.InOutCubic
            }
        }
    }

    Repeater {
        model: control.options

        Item {
            id: segment

            required property int index
            required property string modelData

            x: control.inset + index * control.segmentWidth
            y: control.inset
            width: control.segmentWidth
            height: control.height - control.inset * 2
            scale: segmentTap.pressed ? 0.98 : 1
            activeFocusOnTab: true

            Accessible.role: Accessible.RadioButton
            Accessible.name: modelData
            Accessible.checked: control.currentIndex === index

            Keys.onPressed: function(event) {
                if (event.key !== Qt.Key_Space
                        && event.key !== Qt.Key_Return
                        && event.key !== Qt.Key_Enter) {
                    return
                }
                if (control.currentIndex !== index)
                    control.activated(index)
                event.accepted = true
            }

            Rectangle {
                anchors.fill: parent
                radius: height / 2
                color: "transparent"
                border.width: 1
                border.color: theme.accent
                visible: segmentHover.hovered
            }

            Text {
                anchors.centerIn: parent
                text: modelData
                color: control.currentIndex === index
                    || segmentHover.hovered
                    ? theme.text
                    : theme.muted
                font.family: theme.headingFamily
                font.pixelSize: theme.fontSize(11)
                font.weight: Font.DemiBold

                Behavior on color {
                    ColorAnimation {
                        duration: 120
                    }
                }
            }

            Behavior on scale {
                NumberAnimation {
                    duration: 120
                    easing.type: Easing.OutCubic
                }
            }

            HoverHandler {
                id: segmentHover
            }

            TapHandler {
                id: segmentTap

                onTapped: {
                    segment.forceActiveFocus()
                    if (control.currentIndex !== index)
                        control.activated(index)
                }
            }
        }
    }
}
