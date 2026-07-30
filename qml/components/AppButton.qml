import QtQuick
import QtQuick.Controls

Button {
    id: control

    property color accentColor: theme.accent
    property bool quiet: false

    implicitHeight: 40
    implicitWidth: Math.max(112, contentItem.implicitWidth + 32)
    hoverEnabled: true

    Theme {
        id: theme
    }

    contentItem: Text {
        text: control.text
        color: control.enabled
            ? (control.quiet ? theme.muted : theme.text)
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
            : (control.hovered ? theme.raisedPanel : theme.panel)
        border.width: 1
        border.color: control.enabled
            ? (control.hovered ? control.accentColor : theme.border)
            : theme.border

        scale: control.down ? 0.98 : 1.0
        Behavior on scale {
            NumberAnimation {
                duration: 120
                easing.type: Easing.OutCubic
            }
        }
    }
}
