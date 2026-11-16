import ".." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import "../Theme"

/// Recording output box for the main toolbar.
///
/// Framed like the master out box: "REC OUT" on one line, the L/R trimmed
/// level ladder centred beside it, the start/stop key in the top-right corner
/// and the trim dial on the far right under its GAIN caption.
Item {
    id: root

    /// RECORD_ON and RECORD_SPLIT_CONTINUE are both "recording".
    readonly property bool recording: recStatus.value >= 2

    implicitHeight: 38
    implicitWidth: 216

    Mixxx.ControlProxy {
        id: recStatus

        group: "[Recording]"
        key: "status"
    }
    Mixxx.ControlProxy {
        id: recToggle

        group: "[Recording]"
        key: "toggle_recording"
    }

    Rectangle {
        anchors.fill: parent
        border.color: root.recording ? Theme.danger : Theme.borderStrong
        border.width: 1
        color: Theme.surface2
        radius: Theme.radiusSm

        Behavior on border.color {
            ColorAnimation {
                duration: Theme.motionFast
            }
        }
    }

    Text {
        id: legend

        anchors.left: parent.left
        anchors.leftMargin: Theme.space2
        anchors.verticalCenter: parent.verticalCenter
        color: root.recording ? Theme.danger : Theme.textMuted
        font.capitalization: Font.AllUppercase
        font.family: Theme.fontFamily
        font.letterSpacing: Theme.letterSpacingCaps
        font.pixelSize: Theme.fontSizeXs
        font.weight: Theme.fontWeightDemiBold
        text: qsTr("Rec out")

        Behavior on color {
            ColorAnimation {
                duration: Theme.motionFast
            }
        }
    }

    // ── L/R trimmed level ladder, centred beside the legend ────────────────
    Column {
        id: ladder

        anchors.left: legend.right
        anchors.leftMargin: Theme.space4
        anchors.right: recButton.left
        anchors.rightMargin: Theme.space1 + 2
        anchors.verticalCenter: parent.verticalCenter
        spacing: 3

        Row {
            id: rowL

            height: 8
            spacing: 3

            Text {
                anchors.verticalCenter: parent.verticalCenter
                color: Theme.textMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeXs
                height: 8
                horizontalAlignment: Text.AlignHCenter
                text: qsTr("L")
                verticalAlignment: Text.AlignVCenter
                width: 8
            }
            HorizontalVuMeter {
                group: "[Recording]"
                height: 8
                key: "vu_meter_left"
                muted: !root.recording
                width: ladder.width - 8 - 3 - 3 - 6
            }
        }
        Row {
            id: rowR

            height: 8
            spacing: 3

            Text {
                anchors.verticalCenter: parent.verticalCenter
                color: Theme.textMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeXs
                height: 8
                horizontalAlignment: Text.AlignHCenter
                text: qsTr("R")
                verticalAlignment: Text.AlignVCenter
                width: 8
            }
            HorizontalVuMeter {
                group: "[Recording]"
                height: 8
                key: "vu_meter_right"
                muted: !root.recording
                width: ladder.width - 8 - 3 - 3 - 6
            }
        }
    }

    // ── Start/stop key, top-right of the box ───────────────────────────────
    Rectangle {
        id: recButton

        anchors.right: gainColumn.left
        anchors.rightMargin: Theme.space1 + 2
        anchors.verticalCenter: parent.verticalCenter
        border.color: root.recording ? Theme.danger : Theme.border
        border.width: 1
        color: Theme.surface3
        height: 17
        radius: Theme.radiusSm
        width: 17

        Accessible.name: root.recording ? qsTr("Stop recording") : qsTr("Start recording")
        Accessible.role: Accessible.Button

        Behavior on border.color {
            ColorAnimation {
                duration: Theme.motionFast
            }
        }

        // Gray until a take starts; red is reserved for "recording".
        Rectangle {
            id: recGlyph

            anchors.centerIn: parent
            color: root.recording ? Theme.danger : Theme.textMuted
            height: 8
            radius: root.recording ? 1 : height / 2
            width: 8

            Behavior on color {
                ColorAnimation {
                    duration: Theme.motionFast
                }
            }
            Behavior on radius {
                NumberAnimation {
                    duration: Theme.motionFast
                }
            }
        }
        MouseArea {
            anchors.fill: parent

            // The engine control is momentary: press starts or stops,
            // release clears the button so the next press registers again.
            onPressed: recToggle.value = 1
            onReleased: recToggle.value = 0
        }
    }

    // ── Gain dial on the far right ─────────────────────────────────────────
    Item {
        id: gainColumn

        anchors.bottom: parent.bottom
        anchors.bottomMargin: 1
        anchors.right: parent.right
        anchors.rightMargin: Theme.space2
        anchors.top: parent.top
        anchors.topMargin: 1
        width: 28

        Text {
            id: gainLabel

            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            color: Theme.textMuted
            font.capitalization: Font.AllUppercase
            font.family: Theme.fontFamily
            font.letterSpacing: Theme.letterSpacingCaps
            font.pixelSize: Theme.fontSizeXs
            font.weight: Theme.fontWeightDemiBold
            text: qsTr("Gain")
        }
        Skin.ControlKnob {
            anchors.bottom: parent.bottom
            anchors.horizontalCenter: parent.horizontalCenter
            color: Theme.textPrimary
            group: "[Recording]"
            height: 24
            key: "gain"
            width: 24

            Accessible.name: qsTr("Recording gain")
        }
    }
}
