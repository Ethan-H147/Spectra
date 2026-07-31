import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import QtQuick.Window

ApplicationWindow {
    id: window

    width: 1600
    height: 900
    minimumWidth: 1100
    minimumHeight: 800
    visible: true
    title: "Spectra - Fourier Additive Synth Desktop"
    color: theme.background

    Theme {
        id: theme
    }

    readonly property var pageTitles: [
        "Overview",
        "Synthesizer",
        "Audio Analysis",
        "Harmonic Lab",
        "Spectrogram",
        "Pitch & Shift",
        "Range EQ",
        "Settings"
    ]
    readonly property var pageContexts: [
        "Session / Status",
        "Generated tone / Harmonics",
        "Imported source / Region",
        "Region / Reconstruction",
        "Full file / Fourier models",
        "Full file / Spectral effects",
        "Full file / Frequency shaping",
        "Application / Runtime"
    ]

    FileDialog {
        id: importDialog
        title: "Import audio"
        fileMode: FileDialog.OpenFile
        nameFilters: ["Audio files (*.wav *.mp3 *.ogg *.flac)"]
        onAccepted: spectra.importAudioFile(selectedFile)
    }

    FileDialog {
        id: exportDialog
        title: "Export synthesized tone"
        fileMode: FileDialog.SaveFile
        defaultSuffix: "wav"
        nameFilters: ["WAV audio (*.wav)"]
        onAccepted: spectra.exportSynthFile(selectedFile)
    }

    FileDialog {
        id: harmonicExportDialog
        title: "Export harmonic reconstruction"
        fileMode: FileDialog.SaveFile
        defaultSuffix: "wav"
        nameFilters: ["WAV audio (*.wav)"]
        onAccepted: spectra.exportHarmonicFile(selectedFile)
    }

    FileDialog {
        id: fourierExportDialog
        title: "Export Fourier frame"
        fileMode: FileDialog.SaveFile
        defaultSuffix: "wav"
        nameFilters: ["WAV audio (*.wav)"]
        onAccepted: spectra.exportFourierFile(selectedFile)
    }

    FileDialog {
        id: fullFileExportDialog
        title: "Export full-file reconstruction"
        fileMode: FileDialog.SaveFile
        defaultSuffix: "wav"
        nameFilters: ["WAV audio (*.wav)"]
        onAccepted: spectra.exportFullFileFile(selectedFile)
    }

    FileDialog {
        id: effectExportDialog
        title: "Export processed audio"
        fileMode: FileDialog.SaveFile
        defaultSuffix: "wav"
        nameFilters: ["WAV audio (*.wav)"]
        onAccepted: spectra.exportEffectFile(selectedFile)
    }

    FileDialog {
        id: eqExportDialog
        title: "Export range EQ audio"
        fileMode: FileDialog.SaveFile
        defaultSuffix: "wav"
        nameFilters: ["WAV audio (*.wav)"]
        onAccepted: spectra.exportEqFile(selectedFile)
    }

    Popup {
        id: helpPopup

        anchors.centerIn: Overlay.overlay
        width: Math.min(760, window.width - theme.space4 * 2)
        height: Math.min(640, window.height - theme.space4 * 2)
        modal: true
        focus: true
        padding: theme.space3
        closePolicy: Popup.CloseOnEscape

        background: Rectangle {
            radius: 6
            color: theme.panel
            border.width: 1
            border.color: theme.hoverBorder
        }

        contentItem: ColumnLayout {
            spacing: theme.space2

            RowLayout {
                Layout.fillWidth: true
                spacing: theme.space2

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: theme.space1

                    Text {
                        text: "Help Center"
                        color: theme.text
                        font.family: theme.headingFamily
                        font.pixelSize: theme.fontSize(20)
                        font.weight: Font.DemiBold
                    }

                    Text {
                        text: "Workspace controls and spectral model definitions"
                        color: theme.muted
                        font.family: theme.bodyFamily
                        font.pixelSize: theme.fontSize(11)
                    }
                }

                AppButton {
                    text: "Close"
                    quiet: true
                    onClicked: helpPopup.close()
                }
            }

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 1
                color: theme.border
            }

            ScrollView {
                id: helpScroll
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                ColumnLayout {
                    width: helpScroll.availableWidth
                    spacing: theme.space2

                    GridLayout {
                        Layout.fillWidth: true
                        columns: width < 560 ? 1 : 2
                        columnSpacing: theme.space2
                        rowSpacing: theme.space2

                        Repeater {
                            model: [
                                {
                                    "title": "Overview",
                                    "summary": "Review the imported source and each workspace state.",
                                    "steps": "Import audio, then select a workspace row to continue.",
                                    "details": "The source transport and readiness rows are live controls."
                                },
                                {
                                    "title": "Synthesizer",
                                    "summary": "Build a tone from a fundamental and 16 harmonics.",
                                    "steps": "Choose a preset, adjust the tone, then play or export it.",
                                    "details": "The plots show the waveform envelope, FFT peaks, and pitch."
                                },
                                {
                                    "title": "Audio Analysis",
                                    "summary": "Analyze one time region from the imported source.",
                                    "steps": "Set the region, run the FFT, then inspect its peaks and pitch.",
                                    "details": "Drag to pan, use the wheel to zoom, and reset the view when needed."
                                },
                                {
                                    "title": "Harmonic Lab",
                                    "summary": "Compare harmonic and phase-preserving reconstructions.",
                                    "steps": "Analyze a region first, then play each model from the shared transport.",
                                    "details": "Adjust Top-N before rebuilding or exporting the Fourier frame."
                                },
                                {
                                    "title": "Spectrogram",
                                    "summary": "Build a mono FFT, stereo FFT, or time-varying STFT model.",
                                    "steps": "Choose a model and bin or energy budget, then build it.",
                                    "details": "The page reports memory estimates, progress, retained energy, and output channels."
                                },
                                {
                                    "title": "Pitch & Shift",
                                    "summary": "Move the complete source by a pitch ratio or a fixed frequency offset.",
                                    "steps": "Choose tape speed, pitch shift, or frequency shift; set the amount; then build.",
                                    "details": "Compare the original and processed file before exporting the result."
                                },
                                {
                                    "title": "Range EQ",
                                    "summary": "Shape selected frequency ranges directly on the full-track spectrum.",
                                    "steps": "Drag horizontally to add a band, then drag the band vertically to change its gain.",
                                    "details": "Resize the edges, inspect every band in the list, and build a measured output before export."
                                },
                                {
                                    "title": "Settings",
                                    "summary": "Inspect runtime values and configure display and FFT memory.",
                                    "steps": "Set text scale or the whole-file FFT memory ceiling.",
                                    "details": "The remaining rows report current source, analysis, and shortcut values."
                                }
                            ]

                            Rectangle {
                                required property var modelData

                                Layout.fillWidth: true
                                implicitHeight: helpCardContent.implicitHeight + theme.space3
                                radius: 4
                                color: theme.canvas
                                border.width: 1
                                border.color: theme.border

                                ColumnLayout {
                                    id: helpCardContent
                                    anchors.fill: parent
                                    anchors.margins: 12
                                    spacing: theme.space1

                                    Text {
                                        text: modelData.title
                                        color: theme.text
                                        font.family: theme.headingFamily
                                        font.pixelSize: theme.fontSize(13)
                                        font.weight: Font.DemiBold
                                    }

                                    Text {
                                        Layout.fillWidth: true
                                        text: modelData.summary
                                        color: theme.muted
                                        font.family: theme.bodyFamily
                                        font.pixelSize: theme.fontSize(11)
                                        wrapMode: Text.Wrap
                                    }

                                    Text {
                                        Layout.fillWidth: true
                                        text: modelData.steps
                                        color: theme.text
                                        font.family: theme.bodyFamily
                                        font.pixelSize: theme.fontSize(10.5)
                                        wrapMode: Text.Wrap
                                    }

                                    Text {
                                        Layout.fillWidth: true
                                        text: modelData.details
                                        color: theme.muted
                                        font.family: theme.bodyFamily
                                        font.pixelSize: theme.fontSize(10.5)
                                        wrapMode: Text.Wrap
                                    }
                                }
                            }
                        }
                    }

                    Text {
                        text: "Keyboard"
                        color: theme.text
                        font.family: theme.headingFamily
                        font.pixelSize: theme.fontSize(14)
                        font.weight: Font.DemiBold
                    }

                    Text {
                        Layout.fillWidth: true
                        text: "1–6  Workspaces    F1  Help    F11  Full screen    Space  Play synthesizer"
                        color: theme.muted
                        font.family: theme.bodyFamily
                        font.pixelSize: theme.fontSize(11)
                        wrapMode: Text.Wrap
                    }

                    Text {
                        text: "Model terms"
                        color: theme.text
                        font.family: theme.headingFamily
                        font.pixelSize: theme.fontSize(14)
                        font.weight: Font.DemiBold
                    }

                    Text {
                        Layout.fillWidth: true
                        text: "A harmonic is an integer multiple of a fundamental. "
                            + "An FFT bin stores one frequency sample. "
                            + "An STFT frame applies an FFT to one overlapping time window."
                        color: theme.muted
                        font.family: theme.bodyFamily
                        font.pixelSize: theme.fontSize(11)
                        wrapMode: Text.Wrap
                    }
                }
            }
        }
    }

    Shortcut {
        sequence: "F1"
        onActivated: helpPopup.open()
    }

    Shortcut {
        sequence: "F11"
        onActivated: window.visibility = window.visibility === Window.FullScreen
            ? Window.Windowed
            : Window.FullScreen
    }

    Shortcut {
        sequence: "Space"
        enabled: spectra.currentPage === 1
        onActivated: spectra.playSynth()
    }

    Shortcut {
        sequence: "1"
        onActivated: spectra.setCurrentPage(0)
    }

    Shortcut {
        sequence: "2"
        onActivated: spectra.setCurrentPage(1)
    }

    Shortcut {
        sequence: "3"
        onActivated: spectra.setCurrentPage(2)
    }

    Shortcut {
        sequence: "4"
        onActivated: spectra.setCurrentPage(3)
    }

    Shortcut {
        sequence: "5"
        onActivated: spectra.setCurrentPage(4)
    }

    Shortcut {
        sequence: "6"
        onActivated: spectra.setCurrentPage(5)
    }

    Shortcut {
        sequence: "7"
        onActivated: spectra.setCurrentPage(6)
    }

    Shortcut {
        sequence: "8"
        onActivated: spectra.setCurrentPage(7)
    }

    DropArea {
        anchors.fill: parent
        onDropped: function(drop) {
            if (drop.hasUrls && drop.urls.length > 0) {
                spectra.importAudioFile(drop.urls[0])
                spectra.setCurrentPage(2)
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.preferredWidth: 240
            Layout.fillHeight: true
            color: theme.sidebar
            border.width: 0

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                RowLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 88
                    Layout.leftMargin: theme.space2
                    Layout.rightMargin: theme.space2
                    spacing: theme.space1

                    Image {
                        source: Qt.resolvedUrl("assets/spectra-icon.png")
                        sourceSize.width: 48
                        sourceSize.height: 48
                        fillMode: Image.PreserveAspectFit
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: theme.space1

                        Text {
                            text: "Spectra"
                            color: theme.text
                            font.family: theme.headingFamily
                            font.pixelSize: theme.fontSize(18)
                            font.weight: Font.DemiBold
                        }

                        Text {
                            text: "FOURIER AUDIO LAB"
                            color: theme.muted
                            font.family: theme.bodyFamily
                            font.pixelSize: theme.fontSize(8.5)
                        }
                    }
                }

                Text {
                    Layout.fillWidth: true
                    Layout.leftMargin: theme.space2
                    Layout.topMargin: theme.space1
                    Layout.bottomMargin: theme.space1
                    text: "WORKSPACES"
                    color: theme.quiet
                    font.family: theme.headingFamily
                    font.pixelSize: theme.fontSize(10)
                    font.weight: Font.DemiBold
                }

                Repeater {
                    model: [
                        ["⌂", "Overview"],
                        ["☷", "Synthesizer"],
                        ["⌁", "Audio Analysis"],
                        ["▥", "Harmonic Lab"],
                        ["▦", "Spectrogram"],
                        ["⇄", "Pitch & Shift"],
                        ["≋", "Range EQ"]
                    ]

                    Rectangle {
                        required property int index
                        required property var modelData

                        Layout.fillWidth: true
                        Layout.leftMargin: 8
                        Layout.rightMargin: 8
                        Layout.preferredHeight: 40
                        radius: 3
                        color: spectra.currentPage === index
                            ? theme.raisedPanel
                            : (navHover.hovered ? Qt.lighter(theme.sidebar, 1.15) : "transparent")

                        Rectangle {
                            visible: spectra.currentPage === parent.index
                            anchors.left: parent.left
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            width: 4
                            color: theme.accent
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            spacing: 12

                            Text {
                                text: modelData[0]
                                color: spectra.currentPage === index ? theme.text : theme.muted
                                font.family: theme.bodyFamily
                                font.pixelSize: theme.fontSize(13)
                            }

                            Text {
                                Layout.fillWidth: true
                                text: modelData[1]
                                color: spectra.currentPage === index ? theme.text : theme.muted
                                font.family: theme.bodyFamily
                                font.pixelSize: theme.fontSize(13)
                                elide: Text.ElideRight
                            }

                            Text {
                                text: index + 1
                                color: theme.quiet
                                font.family: theme.bodyFamily
                                font.pixelSize: theme.fontSize(11)
                            }
                        }

                        HoverHandler {
                            id: navHover
                        }

                        TapHandler {
                            onTapped: spectra.setCurrentPage(parent.index)
                        }
                    }
                }

                Item {
                    Layout.fillHeight: true
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.leftMargin: 8
                    Layout.rightMargin: 8
                    Layout.bottomMargin: theme.space2
                    Layout.preferredHeight: 40
                    radius: 3
                    color: spectra.currentPage === 7
                        ? theme.raisedPanel
                        : (settingsHover.hovered ? Qt.lighter(theme.sidebar, 1.15) : "transparent")

                    Rectangle {
                        visible: spectra.currentPage === 7
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        width: 4
                        color: theme.accent
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        spacing: 12

                        Text {
                            text: "⚙"
                            color: spectra.currentPage === 7 ? theme.text : theme.muted
                            font.family: theme.bodyFamily
                            font.pixelSize: theme.fontSize(13)
                        }

                        Text {
                            Layout.fillWidth: true
                            text: "Settings"
                            color: spectra.currentPage === 7 ? theme.text : theme.muted
                            font.family: theme.bodyFamily
                            font.pixelSize: theme.fontSize(13)
                        }
                    }

                    HoverHandler {
                        id: settingsHover
                    }

                    TapHandler {
                        onTapped: spectra.setCurrentPage(7)
                    }
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 88
                color: theme.topbar

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: theme.space4
                    anchors.rightMargin: theme.space3
                    spacing: theme.space2

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: theme.space1

                        Text {
                            text: window.pageTitles[spectra.currentPage]
                            color: theme.text
                            font.family: theme.headingFamily
                            font.pixelSize: theme.fontSize(18)
                            font.weight: Font.DemiBold
                        }

                        Text {
                            text: window.pageContexts[spectra.currentPage]
                            color: theme.muted
                            font.family: theme.bodyFamily
                            font.pixelSize: theme.fontSize(11.5)
                        }
                    }

                    Row {
                        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                        spacing: theme.space1

                        AppButton {
                            text: "Help  F1"
                            quiet: true
                            compact: true
                            onClicked: helpPopup.open()
                        }

                        AppButton {
                            text: window.visibility === Window.FullScreen ? "Exit full screen  F11" : "Full screen  F11"
                            compact: true
                            onClicked: window.visibility = window.visibility === Window.FullScreen
                                ? Window.Windowed
                                : Window.FullScreen
                        }
                    }
                }

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: 1
                    color: theme.border
                }
            }

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                Loader {
                    id: pageLoader
                    anchors.fill: parent
                    anchors.margins: theme.space4
                    sourceComponent: {
                        switch (spectra.currentPage) {
                            case 0: return overviewPage
                            case 1: return synthPage
                            case 2: return analysisPage
                            case 3: return harmonicPage
                            case 4: return spectrogramPage
                            case 5: return pitchShiftPage
                            case 6: return rangeEqPage
                            default: return settingsPage
                        }
                    }
                }

                Connections {
                    target: pageLoader.item
                    ignoreUnknownSignals: true
                    function onImportRequested() {
                        importDialog.open()
                    }
                    function onExportRequested() {
                        exportDialog.open()
                    }
                    function onExportHarmonicRequested() {
                        harmonicExportDialog.open()
                    }
                    function onExportFourierRequested() {
                        fourierExportDialog.open()
                    }
                    function onExportFullFileRequested() {
                        fullFileExportDialog.open()
                    }
                    function onExportEffectRequested() {
                        effectExportDialog.open()
                    }
                    function onExportEqRequested() {
                        eqExportDialog.open()
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 40
                color: theme.topbar

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    height: 1
                    color: theme.border
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: theme.space2
                    anchors.rightMargin: theme.space2
                    spacing: theme.space1

                    Rectangle {
                        implicitWidth: 8
                        implicitHeight: 8
                        radius: 4
                        color: spectra.audioReady ? theme.success : theme.warning
                    }

                    Text {
                        Layout.fillWidth: true
                        text: spectra.statusText
                        color: theme.muted
                        font.family: theme.bodyFamily
                        font.pixelSize: theme.fontSize(10.5)
                        elide: Text.ElideRight
                    }

                    Text {
                        text: "44.1 kHz  |  Mono / stereo  |  Local processing"
                        color: theme.muted
                        font.family: theme.bodyFamily
                        font.pixelSize: theme.fontSize(10.5)
                    }
                }
            }
        }
    }

    Component {
        id: overviewPage
        OverviewPage {}
    }

    Component {
        id: synthPage
        SynthPage {}
    }

    Component {
        id: analysisPage
        AnalysisPage {}
    }

    Component {
        id: harmonicPage
        HarmonicLabPage {}
    }

    Component {
        id: spectrogramPage
        SpectrogramPage {}
    }

    Component {
        id: pitchShiftPage
        PitchShiftPage {}
    }

    Component {
        id: rangeEqPage
        RangeEqPage {}
    }

    Component {
        id: settingsPage
        SettingsPage {}
    }
}
