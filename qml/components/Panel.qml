import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root

    property alias title: titleLabel.text
    property alias subtitle: subtitleLabel.text
    default property alias content: contentSlot.data
    property int contentSpacing: theme.space2

    implicitHeight: panelColumn.implicitHeight + 32
    radius: 5
    color: theme.panel
    border.width: 1
    border.color: theme.border

    Theme {
        id: theme
    }

    ColumnLayout {
        id: panelColumn
        anchors.fill: parent
        anchors.margins: theme.space2
        spacing: theme.space1

        Text {
            id: titleLabel
            Layout.fillWidth: true
            color: theme.text
            font.family: theme.headingFamily
            font.pixelSize: theme.fontSize(16)
            font.weight: Font.DemiBold
            elide: Text.ElideRight
        }

        Text {
            id: subtitleLabel
            Layout.fillWidth: true
            visible: text.length > 0
            color: theme.muted
            font.family: theme.bodyFamily
            font.pixelSize: theme.fontSize(12)
            elide: Text.ElideRight
        }

        Item {
            Layout.fillWidth: true
            implicitHeight: root.contentSpacing - theme.space1
            visible: contentSlot.children.length > 0
        }

        ColumnLayout {
            id: contentSlot
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: theme.space1
        }
    }
}
