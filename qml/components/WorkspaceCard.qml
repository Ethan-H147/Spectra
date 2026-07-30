import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root

    required property string title
    required property string value
    required property string detail
    required property string statusText
    property color stateColor: theme.quiet
    signal activated()

    implicitHeight: contentColumn.implicitHeight + 32
    radius: 5
    color: tapHandler.pressed
        ? Qt.lighter(theme.raisedPanel, 1.12)
        : (hoverHandler.hovered ? theme.raisedPanel : theme.panel)
    border.width: 1
    border.color: hoverHandler.hovered ? theme.hoverBorder : theme.border

    Theme {
        id: theme
    }

    ColumnLayout {
        id: contentColumn
        anchors.fill: parent
        anchors.margins: theme.space2
        spacing: theme.space1

        RowLayout {
            Layout.fillWidth: true
            spacing: theme.space1

            Text {
                Layout.fillWidth: true
                text: root.title
                color: theme.text
                font.family: theme.headingFamily
                font.pixelSize: theme.fontSize(16)
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }

            Rectangle {
                implicitWidth: stateLabel.implicitWidth + 16
                implicitHeight: 24
                radius: 3
                color: Qt.rgba(root.stateColor.r, root.stateColor.g, root.stateColor.b, 0.12)

                Text {
                    id: stateLabel
                    anchors.centerIn: parent
                    text: root.statusText
                    color: root.stateColor
                    font.family: theme.headingFamily
                    font.pixelSize: theme.fontSize(10)
                    font.weight: Font.DemiBold
                }
            }
        }

        Text {
            Layout.fillWidth: true
            Layout.topMargin: theme.space1
            text: root.value
            color: theme.text
            font.family: theme.headingFamily
            font.pixelSize: theme.fontSize(20)
            font.weight: Font.DemiBold
            elide: Text.ElideRight
        }

        Text {
            Layout.fillWidth: true
            text: root.detail
            color: theme.muted
            font.family: theme.bodyFamily
            font.pixelSize: theme.fontSize(12)
            elide: Text.ElideRight
        }

        Item {
            Layout.fillHeight: true
            implicitHeight: theme.space2
        }

        Text {
            text: "Open workspace  ›"
            color: hoverHandler.hovered ? theme.accent : theme.muted
            font.family: theme.headingFamily
            font.pixelSize: theme.fontSize(12)
            font.weight: Font.DemiBold
        }
    }

    HoverHandler {
        id: hoverHandler
    }

    TapHandler {
        id: tapHandler
        onTapped: root.activated()
    }
}
