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
/// mode and cueing keys cycle their controls and PAS taps to toggle or holds
/// to listen to the vinyl input.
Item {
    id: root

    /// The record-end warning blinks 2 and 0; never show the blink as "off".
    readonly property int displayStatus: vinylEnabled.value > 0 ? (root.status > 0 ? root.status : root.lastStatus) : 0
    required property string group
    /// Last non-zero vinylcontrol_status, held through the record-end blink.
    property int lastStatus: 0
    readonly property int status: Math.round(vinylStatus.value)
    readonly property color statusColor: {
        switch (root.displayStatus) {
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
    /// Status 0 means disabled or waiting for the first timecode status.
    /// Live timecode reports 1; a needle skip reports 3.
    readonly property string statusText: {
        switch (root.displayStatus) {
        case 1:
            return qsTr("ACTIVE");
        case 2:
            return qsTr("RECORD END");
        case 3:
            return qsTr("NEEDLE SKIP");
        default:
            return vinylEnabled.value > 0 ? qsTr("—") : qsTr("OFF");
        }
    }

    implicitHeight: 20

    onStatusChanged: {
        if (root.status > 0) {
            root.lastStatus = root.status;
        }
    }

    Mixxx.ControlProxy {
        id: vinylEnabled

        group: root.group
        key: "vinylcontrol_enabled"

        // Do not carry a stale RECORD END into the next enable.
        onValueChanged: root.lastStatus = 0
    }
    Mixxx.ControlProxy {
        id: vinylStatus

        group: root.group
        key: "vinylcontrol_status"
    }
    RowLayout {
        anchors.fill: parent
        spacing: Theme.space1

        Skin.Button {
            id: vinylKey

            Accessible.checkable: true
            Accessible.checked: vinylEnabled.value > 0
            Accessible.description: qsTr("Timecode vinyl control for this deck")
            Accessible.name: qsTr("Vinyl control")
            Accessible.role: Accessible.CheckBox
            Layout.fillHeight: true
            activeColor: Theme.textOnAccent
            highlight: vinylEnabled.value > 0
            text: qsTr("VINYL")

            background: Rectangle {
                border.color: vinylKey.activeFocus ? Theme.focusRing : vinylKey.highlight ? root.statusColor : vinylKey.hovered ? Theme.borderStrong : Theme.border
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
            Accessible.role: Accessible.StaticText
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

            /// True while the pointer gesture is driving the power window, so
            /// the click emitted after a release does not toggle a second time.
            property bool pressHandled: false

            Accessible.checkable: true
            Accessible.checked: passthroughBehavior.isActive
            Accessible.description: qsTr("Tap to toggle, hold to listen to the vinyl input")
            Accessible.name: qsTr("Passthrough")
            Accessible.role: Accessible.CheckBox
            Layout.fillHeight: true
            activeColor: Theme.textOnAccent
            highlight: passthroughBehavior.isActive
            text: qsTr("PASS")

            // Keyboard and accessibility activation do not emit press/release.
            onClicked: {
                if (!passthroughKey.pressHandled)
                    passthroughBehavior.toggleControl();
            }
            onPressed: {
                passthroughKey.pressHandled = true;
                passthroughBehavior.pressPrimary();
            }
            onReleased: {
                passthroughBehavior.releasePrimary();
                Qt.callLater(function () {
                    passthroughKey.pressHandled = false;
                });
            }

            Skin.ControlProxyButtonBehavior {
                id: passthroughBehavior

                group: root.group
                handlePointerInput: false
                key: "passthrough"
                powerWindow: true
            }
        }
        Item {
            Layout.fillWidth: true
        }
    }
}
