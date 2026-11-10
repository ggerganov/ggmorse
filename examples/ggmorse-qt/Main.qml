import QtQuick
import QtCore
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ApplicationWindow {
    id: window

    width: 980
    height: 640
    minimumWidth: 760
    minimumHeight: 520
    visible: true
    title: "GGMorse"

    Material.theme: Material.Dark

    Settings {
        category: "ui"
        property alias settingsVisible: settingsButton.checked
    }
    Material.accent: "#2dd4bf"
    Material.primary: "#0f172a"
    color: "#0b1220"

    readonly property color cardColor: "#131c2e"
    readonly property color borderColor: "#22304a"
    readonly property color mutedText: "#8fa0bd"
    readonly property color accentColor: "#2dd4bf"

    component Card: Rectangle {
        color: window.cardColor
        radius: 12
        border.color: window.borderColor
        border.width: 1
    }

    component SectionLabel: Label {
        color: window.mutedText
        font.pixelSize: 12
        font.weight: Font.DemiBold
        font.letterSpacing: 1.2
        font.capitalization: Font.AllUppercase
    }

    header: Rectangle {
        height: 64
        color: "#0f172a"

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 24
            anchors.rightMargin: 24
            spacing: 16

            Label {
                text: "GGMorse"
                font.pixelSize: 22
                font.weight: Font.Bold
            }

            Rectangle {
                radius: height / 2
                implicitHeight: 26
                implicitWidth: statusRow.implicitWidth + 24
                color: decoder.running ? "#0f3b36" : "#1e293b"

                Row {
                    id: statusRow
                    anchors.centerIn: parent
                    spacing: 8

                    Rectangle {
                        width: 8
                        height: 8
                        radius: 4
                        anchors.verticalCenter: parent.verticalCenter
                        color: decoder.running ? window.accentColor : window.mutedText

                        SequentialAnimation on opacity {
                            running: decoder.running
                            loops: Animation.Infinite
                            NumberAnimation { to: 0.3; duration: 700 }
                            NumberAnimation { to: 1.0; duration: 700 }
                            onStopped: parent.opacity = 1.0
                        }
                    }

                    Label {
                        text: decoder.running ? qsTr("Listening") : qsTr("Idle")
                        font.pixelSize: 12
                        color: decoder.running ? window.accentColor : window.mutedText
                    }
                }
            }

            Item { Layout.fillWidth: true }

            ToolButton {
                id: settingsButton
                text: "\u2699"
                font.pixelSize: 22
                checkable: true
                checked: true
                ToolTip.visible: hovered
                ToolTip.text: checked ? qsTr("Hide settings") : qsTr("Show settings")
            }

            Button {
                text: decoder.running ? qsTr("Stop") : qsTr("Start listening")
                highlighted: !decoder.running
                Material.accent: decoder.running ? "#f87171" : window.accentColor
                onClicked: decoder.running ? decoder.stop() : decoder.start()
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 20

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 20

            Rectangle {
                Layout.fillWidth: true
                visible: decoder.error.length > 0
                implicitHeight: 40
                radius: 8
                color: "#3b1d24"
                border.color: "#7f1d1d"

                Label {
                    anchors.fill: parent
                    anchors.leftMargin: 14
                    verticalAlignment: Text.AlignVCenter
                    text: decoder.error
                    color: "#fca5a5"
                    elide: Text.ElideRight
                }
            }

            Card {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredHeight: 1

                SectionLabel {
                    x: 16
                    y: 12
                    text: qsTr("Spectrogram")
                }

                SectionLabel {
                    anchors.right: parent.right
                    anchors.rightMargin: 16
                    y: 12
                    font.capitalization: Font.MixedCase
                    text: qsTr("TONE") + " " + (decoder.running && decoder.pitch > 0 ? Math.round(decoder.pitch) + " Hz" : "—")
                        + "     " + qsTr("SPEED") + " " + (decoder.running && decoder.speed > 0 ? decoder.speed.toFixed(1) + " WPM" : "—")
                }

                Rectangle {
                    id: spectrogramArea
                    anchors.fill: parent
                    anchors.margins: 16
                    anchors.topMargin: 38
                    anchors.rightMargin: 28
                    color: "black"
                    radius: 4
                    clip: true

                    Image {
                        anchors.fill: parent
                        cache: false
                        smooth: false
                        fillMode: Image.Stretch
                        source: decoder.frame > 0 ? "image://spectrogram/" + decoder.frame : ""
                    }
                }

                Canvas {
                    id: pitchMarker
                    width: 10
                    height: 12
                    x: spectrogramArea.x + spectrogramArea.width + 4
                    y: spectrogramArea.y + decoder.pitchPosition * spectrogramArea.height - height / 2
                    visible: decoder.running && decoder.pitchPosition >= 0

                    onPaint: {
                        const ctx = getContext("2d")
                        ctx.clearRect(0, 0, width, height)
                        ctx.fillStyle = "#facc15"
                        ctx.beginPath()
                        ctx.moveTo(0, height / 2)
                        ctx.lineTo(width, 0)
                        ctx.lineTo(width, height)
                        ctx.closePath()
                        ctx.fill()
                    }
                }
            }

            Card {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredHeight: 1

                SectionLabel {
                    x: 16
                    y: 12
                    text: qsTr("Decoded")
                }

                Row {
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 8
                    spacing: 4

                    Button {
                        flat: true
                        text: qsTr("Copy")
                        enabled: decoder.text.length > 0
                        onClicked: {
                            output.selectAll()
                            output.copy()
                            output.deselect()
                        }
                    }
                    Button {
                        flat: true
                        text: qsTr("Clear")
                        enabled: decoder.text.length > 0
                        onClicked: decoder.clear()
                    }
                }

                ScrollView {
                    id: scroll
                    anchors.fill: parent
                    anchors.margins: 16
                    anchors.topMargin: 46

                    TextArea {
                        id: output
                        readOnly: true
                        wrapMode: TextEdit.Wrap
                        selectByMouse: true
                        text: decoder.text
                        font.family: "monospace"
                        font.pixelSize: 22
                        color: "#e2e8f0"
                        background: null
                        placeholderText: decoder.running ? qsTr("Waiting for Morse signal…") : qsTr("Press “Start listening” to begin decoding")

                        onTextChanged: Qt.callLater(function() {
                            scroll.ScrollBar.vertical.position = 1.0 - scroll.ScrollBar.vertical.size
                        })
                    }
                }
            }
        }

        Card {
            visible: settingsButton.checked
            Layout.preferredWidth: 300
            Layout.fillHeight: true

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 14

                SectionLabel { text: qsTr("Input device") }

                ComboBox {
                    Layout.fillWidth: true
                    model: decoder.devices
                    currentIndex: decoder.deviceIndex
                    onActivated: (index) => decoder.deviceIndex = index
                }

                SectionLabel {
                    text: qsTr("Tone frequency")
                    Layout.topMargin: 8
                }

                RowLayout {
                    Layout.fillWidth: true

                    Switch {
                        id: autoFrequency
                        text: qsTr("Auto")
                        checked: decoder.frequency <= 0
                        onToggled: decoder.frequency = checked ? -1 : freqSlider.value
                    }
                    Item { Layout.fillWidth: true }
                    Label {
                        text: Math.round(freqSlider.value) + " Hz"
                        color: autoFrequency.checked ? window.mutedText : "white"
                    }
                }

                Slider {
                    id: freqSlider
                    Layout.fillWidth: true
                    from: 200
                    to: 1200
                    stepSize: 10
                    value: decoder.frequency > 0 ? decoder.frequency : 600
                    enabled: !autoFrequency.checked
                    onMoved: decoder.frequency = value
                }

                SectionLabel {
                    text: qsTr("Speed")
                    Layout.topMargin: 8
                }

                RowLayout {
                    Layout.fillWidth: true

                    Switch {
                        id: autoSpeed
                        text: qsTr("Auto")
                        checked: decoder.wpm <= 0
                        onToggled: decoder.wpm = checked ? -1 : wpmSlider.value
                    }
                    Item { Layout.fillWidth: true }
                    Label {
                        text: Math.round(wpmSlider.value) + " WPM"
                        color: autoSpeed.checked ? window.mutedText : "white"
                    }
                }

                Slider {
                    id: wpmSlider
                    Layout.fillWidth: true
                    from: 5
                    to: 60
                    stepSize: 1
                    value: decoder.wpm > 0 ? decoder.wpm : 20
                    enabled: !autoSpeed.checked
                    onMoved: decoder.wpm = value
                }

                SectionLabel {
                    text: qsTr("Filters")
                    Layout.topMargin: 8
                }

                Switch {
                    text: qsTr("High-pass")
                    checked: decoder.highPass
                    onToggled: decoder.highPass = checked
                }

                Switch {
                    text: qsTr("Low-pass")
                    checked: decoder.lowPass
                    onToggled: decoder.lowPass = checked
                }

                Item { Layout.fillHeight: true }
            }
        }
    }
}
