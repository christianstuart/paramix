import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import QtQuick.Layouts
import ".." as Skin
import "../Theme"

/// Compact vinyl control row for a deck.
///
/// Restores the classic deck strip: the VINYL key requests
/// `vinylcontrol_enabled` and is filled with the `vinylcontrol_status` colour
/// (green = control active, amber = record end, red = needle skip), while the
/// mode and cueing keys cycle their controls and PAS toggles passthrough.
Item {
    id: root

    required property string group
    readonly property int status: Math.round(vinylStatus.value)
    readonly property color statusColor: {
        switch (root.status) {
        case 1:
            return Theme.positive;
        case 2:
            return Theme.warning;
        case 3:
            return Theme.danger;
        default:
            return Theme.textMuted;
        }
    }
    /// Status 0 means disabled, or enabled with no usable timecode.
    readonly property string statusText: {
        switch (root.status) {
        case 1:
            return qsTr("ACTIVE");
        case 2:
            return qsTr("RECORD END");
        case 3:
            return qsTr("NEEDLE SKIP");
        default:
            return vinylEnabled.value > 0 ? qsTr("NO SIGNAL") : qsTr("OFF");
        }
    }

    implicitHeight: 20

    Mixxx.ControlProxy {
        id: vinylEnabled

        group: root.group
        key: "vinylcontrol_enabled"
    }
    Mixxx.ControlProxy {
        id: vinylStatus

        group: root.group
        key: "vinylcontrol_status"
    }
    Mixxx.ControlProxy {
        id: passthrough

        group: root.group
        key: "passthrough"
    }
    RowLayout {
        anchors.fill: parent
        spacing: Theme.space1

        Skin.Button {
            id: vinylKey

            Accessible.description: qsTr("Timecode vinyl control for this deck")
            Accessible.name: qsTr("Vinyl control")
            Layout.fillHeight: true
            activeColor: Theme.textOnAccent
            highlight: vinylEnabled.value > 0
            text: qsTr("VINYL")

            background: Rectangle {
                border.color: vinylKey.highlight ? root.statusColor : vinylKey.hovered || vinylKey.activeFocus ? Theme.borderStrong : Theme.border
                border.width: 1
                color: vinylKey.highlight ? root.statusColor : vinylKey.pressed ? Theme.surface3 : vinylKey.hovered ? Theme.surfaceHover : Theme.surface2
                radius: Theme.radiusSm

                Behavior on color {
                    ColorAnimation {
                        duration: Theme.motionFast
                    }
                }
            }

            onClicked: vinylEnabled.value = vinylEnabled.value > 0 ? 0 : 1
        }
        Text {
            Accessible.name: qsTr("Vinyl status: %1").arg(root.statusText)
            Layout.alignment: Qt.AlignVCenter
            Layout.preferredWidth: 84
            color: root.statusColor
            elide: Text.ElideRight
            font.capitalization: Font.AllUppercase
            font.family: Theme.fontFamily
            font.letterSpacing: Theme.letterSpacingCaps
            font.pixelSize: Theme.fontSizeXs
            font.weight: Theme.fontWeightDemiBold
            text: root.statusText
            verticalAlignment: Text.AlignVCenter
        }
        Skin.Button {
            id: modeKey

            Accessible.description: qsTr("Absolute follows the needle position, relative follows its speed, constant holds the last steady speed")
            Accessible.name: qsTr("Vinyl control mode: %1").arg(modeCycle.label)
            Layout.fillHeight: true
            normalColor: Theme.textMuted
            text: modeCycle.label

            onClicked: modeCycle.cycle()

            Skin.ControlCycleButtonBehavior {
                id: modeCycle

                group: root.group
                handlePointerInput: false
                key: "vinylcontrol_mode"
                numberStates: 3
                stateLabels: [qsTr("ABS"), qsTr("REL"), qsTr("CONST")]
            }
        }
        Skin.Button {
            id: cueingKey

            Accessible.description: qsTr("How cue points are treated in relative mode")
            Accessible.name: qsTr("Vinyl cueing mode: %1").arg(cueingCycle.label)
            Layout.fillHeight: true
            normalColor: Theme.textMuted
            text: cueingCycle.label

            onClicked: cueingCycle.cycle()

            Skin.ControlCycleButtonBehavior {
                id: cueingCycle

                group: root.group
                handlePointerInput: false
                key: "vinylcontrol_cueing"
                numberStates: 3
                stateLabels: [qsTr("OFF"), qsTr("CUE"), qsTr("HOT")]
            }
        }
        Skin.Button {
            id: passthroughKey

            Accessible.description: qsTr("Play the audio arriving on the vinyl input directly")
            Accessible.name: qsTr("Passthrough")
            Layout.fillHeight: true
            activeColor: Theme.textOnAccent
            highlight: passthrough.value > 0
            text: qsTr("PASS")

            onClicked: passthrough.value = passthrough.value > 0 ? 0 : 1
        }
        Item {
            Layout.fillWidth: true
        }
    }
}
