import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Control {
    id: root

    property string message: ""
    property string busyMessage: ""
    property bool busy: false
    property real progress: 0
    property real maximumWidth: 520

    property bool shown: false
    property bool persistent: false
    property int tone: 0
    property string displayedMessage: ""

    readonly property color toneColor: tone === 3
        ? theme.danger
        : (tone === 2
            ? theme.warning
            : (tone === 1 ? theme.success : theme.accent))

    implicitWidth: Math.min(
        maximumWidth,
        Math.max(220, noticeContent.implicitWidth + leftPadding + rightPadding))
    implicitHeight: 42
    leftPadding: 13
    rightPadding: persistent ? 8 : 13
    topPadding: 0
    bottomPadding: 0
    visible: shown || opacity > 0.01
    opacity: shown ? 1 : 0
    scale: shown ? 1 : 0.98
    clip: true

    Accessible.name: displayedMessage
    Accessible.role: Accessible.StaticText

    function lower(textValue) {
        return textValue.trim().toLowerCase()
    }

    function isError(textValue) {
        const value = lower(textValue)
        return value.indexOf("could not") === 0
            || value.indexOf("failed") >= 0
            || value.indexOf("unavailable") >= 0
            || value.indexOf("did not complete") >= 0
            || value.indexOf("exceeds the configured limit") >= 0
            || value.indexOf("above the") >= 0
            || value.indexOf("no audio device") === 0
    }

    function isGuidance(textValue) {
        const value = lower(textValue)
        return value.indexOf("import audio before") === 0
            || value.indexOf("add or enable") === 0
            || value.indexOf("build the spectrogram before") === 0
    }

    function isUsefulConfirmation(textValue) {
        const value = lower(textValue)
        return value.indexOf("exported ") === 0
            || value.indexOf("copied diagnostics") === 0
    }

    function conciseText(textValue) {
        const value = lower(textValue)
        if (value.indexOf("copied diagnostics") === 0)
            return "Diagnostics copied"
        if (value === "building range eq output")
            return "Building EQ output"
        if (value === "building whole-file spectrogram")
            return "Building spectrogram"
        if (value.indexOf("reconstructing each stft frame") === 0)
            return "Building STFT reconstruction"
        return textValue.trim()
    }

    function showBusy() {
        dismissTimer.stop()
        persistent = false
        tone = 0
        displayedMessage = conciseText(busyMessage)
        shown = displayedMessage.length > 0
    }

    function considerMessage(textValue) {
        if (busy) {
            showBusy()
            return
        }

        const clean = textValue.trim()
        dismissTimer.stop()

        if (isError(clean)) {
            tone = 3
            persistent = true
            displayedMessage = conciseText(clean)
            shown = true
            return
        }

        if (isGuidance(clean)) {
            tone = 2
            persistent = false
            displayedMessage = conciseText(clean)
            shown = true
            dismissTimer.interval = 4800
            dismissTimer.restart()
            return
        }

        if (isUsefulConfirmation(clean)) {
            tone = 1
            persistent = false
            displayedMessage = conciseText(clean)
            shown = true
            dismissTimer.interval = 2800
            dismissTimer.restart()
            return
        }

        shown = false
    }

    onBusyChanged: {
        if (busy)
            showBusy()
        else
            considerMessage(message)
    }
    onBusyMessageChanged: {
        if (busy)
            showBusy()
    }
    onMessageChanged: considerMessage(message)

    Component.onCompleted: considerMessage(message)

    Behavior on opacity {
        NumberAnimation {
            duration: root.shown ? 180 : 130
            easing.type: Easing.OutCubic
        }
    }

    Behavior on scale {
        NumberAnimation {
            duration: root.shown ? 180 : 130
            easing.type: Easing.OutCubic
        }
    }

    Timer {
        id: dismissTimer
        repeat: false
        onTriggered: root.shown = false
    }

    contentItem: RowLayout {
        id: noticeContent
        spacing: 10

        BusyIndicator {
            Layout.preferredWidth: 17
            Layout.preferredHeight: 17
            padding: 0
            running: root.busy && root.shown
            visible: root.busy
        }

        Rectangle {
            Layout.preferredWidth: 17
            Layout.preferredHeight: 17
            radius: 8.5
            color: Qt.rgba(
                root.toneColor.r,
                root.toneColor.g,
                root.toneColor.b,
                0.14)
            border.width: 1
            border.color: root.toneColor
            visible: !root.busy

            Text {
                anchors.centerIn: parent
                anchors.verticalCenterOffset: root.tone === 1 ? -0.5 : -1
                text: root.tone === 1 ? "✓" : "!"
                color: root.toneColor
                font.family: theme.bodyFamily
                font.pixelSize: theme.fontSize(root.tone === 1 ? 9 : 10)
                font.weight: Font.Bold
            }
        }

        Text {
            Layout.maximumWidth: Math.max(120, root.maximumWidth - 150)
            text: root.displayedMessage
            color: theme.text
            font.family: theme.bodyFamily
            font.pixelSize: theme.fontSize(10.5)
            elide: Text.ElideRight
        }

        Text {
            visible: root.busy
            text: Math.round(Math.max(0, Math.min(1, root.progress)) * 100) + "%"
            color: theme.muted
            font.family: theme.bodyFamily
            font.pixelSize: theme.fontSize(10)
        }

        Button {
            id: dismissButton
            Layout.preferredWidth: 26
            Layout.preferredHeight: 26
            visible: root.persistent
            hoverEnabled: true
            padding: 0

            Accessible.name: "Dismiss message"
            ToolTip.visible: hovered
            ToolTip.text: "Dismiss"
            ToolTip.delay: 500

            onClicked: root.shown = false

            contentItem: Text {
                text: "×"
                color: dismissButton.hovered ? theme.text : theme.muted
                font.family: theme.bodyFamily
                font.pixelSize: theme.fontSize(14)
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }

            background: Rectangle {
                radius: 4
                color: dismissButton.down
                    ? theme.hoverBorder
                    : (dismissButton.hovered ? theme.raisedPanel : "transparent")
            }

            scale: down ? 0.96 : 1

            Behavior on scale {
                NumberAnimation {
                    duration: 120
                    easing.type: Easing.OutCubic
                }
            }
        }
    }

    background: Rectangle {
        radius: 6
        color: theme.raisedPanel
        border.width: 1
        border.color: root.busy
            ? Qt.rgba(theme.accent.r, theme.accent.g, theme.accent.b, 0.62)
            : Qt.rgba(
                root.toneColor.r,
                root.toneColor.g,
                root.toneColor.b,
                root.tone === 3 ? 0.72 : 0.48)

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.leftMargin: 2
            anchors.rightMargin: 2
            anchors.bottomMargin: 2
            height: 2
            radius: 1
            color: theme.accent
            visible: root.busy
            transform: Scale {
                origin.x: 0
                origin.y: 1
                xScale: Math.max(0, Math.min(1, root.progress))
                yScale: 1

                Behavior on xScale {
                    NumberAnimation {
                        duration: 120
                        easing.type: Easing.Linear
                    }
                }
            }
        }
    }
}
