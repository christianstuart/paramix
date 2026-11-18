import ".." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import "../Theme"

/// Recording output box for the main toolbar.
///
/// Framed like the master out box: "REC OUT" on one line, the L/R trimmed
/// level ladder centered beside it, the start/stop key in the top-right corner
/// and the trim dial on the far right under its GAIN caption.
Item {
    id: root

    /// RECORD_ON and RECORD_SPLIT_CONTINUE are both "recording".
    readonly property bool recording: recStatus.value >= 2
    /// The stats line yields to the L/R ladder: at the largest permitted font
    /// scale its natural width would leave the meter only a few px. The line
    /// elides here so the ladder keeps a usable minimum in the fixed 216 px box.
    readonly property int statsMaxWidth: 64

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
    Column {
        id: legendColumn

        anchors.left: parent.left
        anchors.leftMargin: Theme.space2
        anchors.verticalCenter: parent.verticalCenter
        spacing: 0
        width: Math.max(root.statsMaxWidth, legend.implicitWidth)

        Text {
            id: legend

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
        // Take time and bytes written. The proxy already formats both; the box
        // only has to show them, so a live take is readable without opening the
        // recordings view.
        Text {
            id: recordingStats

            Accessible.name: qsTr("Recording time %1, file size %2").arg(Mixxx.Recording.durationText).arg(Mixxx.Recording.fileSizeText)
            color: root.recording ? Theme.textSecondary : Theme.textMuted
            elide: Text.ElideRight
            font.family: Theme.fontFamilyMono
            font.pixelSize: Theme.fontSizeXs - 1
            text: qsTr("%1 \u00b7 %2").arg(Mixxx.Recording.durationText).arg(Mixxx.Recording.fileSizeText)
            width: root.statsMaxWidth
        }
    }

    // ── L/R trimmed level ladder, centered beside the legend ────────────────
    Column {
        id: ladder

        anchors.left: legendColumn.right
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

        Accessible.name: root.recording ? qsTr("Stop recording") : qsTr("Start recording")
        Accessible.role: Accessible.Button
        anchors.right: gainColumn.left
        anchors.rightMargin: Theme.space1 + 2
        anchors.verticalCenter: parent.verticalCenter
        border.color: root.recording ? Theme.danger : Theme.border
        border.width: 1
        color: Theme.surface3
        height: 17
        radius: Theme.radiusSm
        width: 17

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
            Accessible.name: qsTr("Recording gain")
            anchors.bottom: parent.bottom
            anchors.horizontalCenter: parent.horizontalCenter
            color: Theme.textPrimary
            group: "[Recording]"
            height: 24
            key: "gain"
            width: 24
        }
    }
}
