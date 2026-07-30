import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root

    required property string label
    required property string value
    required property string detail
    property color stateColor: theme.quiet

    implicitHeight: contentColumn.implicitHeight + 24
    radius: 5
    color: theme.panel
    border.width: 1
    border.color: theme.border

    Theme {
        id: theme
    }

    ColumnLayout {
        id: contentColumn
        anchors.fill: parent
        anchors.margins: 12
        spacing: theme.space1

        RowLayout {
            Layout.fillWidth: true
            spacing: theme.space1

            Rectangle {
                implicitWidth: 8
                implicitHeight: 8
                radius: 4
                color: root.stateColor
            }

            Text {
                Layout.fillWidth: true
                text: root.label
                color: theme.muted
                font.family: theme.headingFamily
                font.pixelSize: theme.fontSize(12)
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }
        }

        Text {
            Layout.fillWidth: true
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
    }
}
