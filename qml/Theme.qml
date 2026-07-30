import QtQuick

QtObject {
    readonly property real legacyTypeScale: 1.85 * spectra.textScale

    readonly property int space1: 8
    readonly property int space2: 16
    readonly property int space3: 24
    readonly property int space4: 32

    readonly property color background: "#1E1E20"
    readonly property color sidebar: "#19191B"
    readonly property color topbar: "#1D1D1F"
    readonly property color panel: "#242427"
    readonly property color raisedPanel: "#29292D"
    readonly property color canvas: "#18181B"
    readonly property color border: "#39393E"
    readonly property color hoverBorder: "#4E545E"
    readonly property color text: "#F5F5F7"
    readonly property color muted: "#A6A6AC"
    readonly property color quiet: "#76767D"
    readonly property color accent: "#2680EB"
    readonly property color success: "#46BE78"
    readonly property color warning: "#E5A03E"
    readonly property color danger: "#E2584E"

    readonly property string bodyFamily: "Segoe UI"
    readonly property string headingFamily: "Segoe UI Semibold"

    function fontSize(legacySize) {
        return Math.round(legacySize * legacyTypeScale)
    }
}
