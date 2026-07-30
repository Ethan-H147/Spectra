import QtQuick
import QtQuick.Controls

Button {
    id: control

    property color accentColor: theme.accent
    property bool quiet: false
    property bool compact: false
    property bool selected: false

    implicitHeight: compact ? 32 : 40
    implicitWidth: compact
        ? contentItem.implicitWidth + 24
        : Math.max(112, contentItem.implicitWidth + 32)
    hoverEnabled: true

    Theme {
        id: theme
    }

    contentItem: Text {
        text: control.text
        color: control.enabled
            ? (control.selected
                ? theme.text
                : (control.quiet ? theme.muted : theme.text))
            : theme.quiet
        font.family: theme.headingFamily
        font.pixelSize: theme.fontSize(12)
        font.weight: Font.DemiBold
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        radius: 4
        color: control.down
            ? Qt.lighter(theme.raisedPanel, 1.12)
            : (control.selected
                ? theme.raisedPanel
                : (control.hovered ? theme.raisedPanel : theme.panel))
        border.width: 1
        border.color: control.enabled
            ? (control.hovered
                ? control.accentColor
                : (control.selected ? theme.hoverBorder : theme.border))
            : theme.border

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.leftMargin: 4
            anchors.rightMargin: 4
            height: 2
            radius: 1
            color: control.accentColor
            visible: control.selected
        }

        scale: control.down ? 0.98 : 1.0
        Behavior on scale {
            NumberAnimation {
                duration: 120
                easing.type: Easing.OutCubic
            }
        }
    }
}
