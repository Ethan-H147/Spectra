import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root

    required property string label
    required property string value
    property color stateColor: theme.quiet
    property bool interactive: true
    signal activated()

    implicitHeight: 40
    radius: 4
    color: tapHandler.pressed
        ? Qt.lighter(theme.raisedPanel, 1.1)
        : (hoverHandler.hovered ? theme.raisedPanel : theme.panel)
    border.width: 1
    border.color: hoverHandler.hovered ? theme.hoverBorder : theme.border

    Theme {
        id: theme
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: theme.space2
        anchors.rightMargin: theme.space2
        spacing: theme.space1

        Rectangle {
            implicitWidth: 8
            implicitHeight: 8
            radius: 2
            color: root.stateColor
        }

        Text {
            Layout.preferredWidth: Math.max(136, root.width * 0.38)
            text: root.label
            color: theme.text
            font.family: theme.headingFamily
            font.pixelSize: theme.fontSize(12)
            font.weight: Font.DemiBold
            elide: Text.ElideRight
        }

        Text {
            Layout.fillWidth: true
            text: root.value
            color: theme.muted
            font.family: theme.bodyFamily
            font.pixelSize: theme.fontSize(11.5)
            elide: Text.ElideRight
        }

        Text {
            text: "›"
            visible: root.interactive
            color: hoverHandler.hovered ? theme.accent : theme.muted
            font.family: theme.bodyFamily
            font.pixelSize: theme.fontSize(16)
        }
    }

    HoverHandler {
        id: hoverHandler
        enabled: root.interactive
    }

    TapHandler {
        id: tapHandler
        enabled: root.interactive
        onTapped: root.activated()
    }
}
