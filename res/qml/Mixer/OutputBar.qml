import ".." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import QtQuick.Layouts
import "../Theme"

/// Master and headphone controls for the mixer.
///
/// The narrow mixer column cannot fit these without clipping, so they live in a
/// full-width bar directly below the deck/mixer row: the output knobs on the
/// far left, the crossfader centered in the bar, and a horizontal master level
/// meter on the right.
Item {
    id: root

    property bool show4decks: false
    readonly property int sideMargin: Theme.space3
    readonly property int tileWidth: root.width < 1200 ? 56 : 64

    // 42 is the practical floor for the bar: the 38 px output tiles plus the
    // two 2 px section rules. It was 54 while the crossfader well carried
    // 16 px assignment keys and the tiles had more padding.
    implicitHeight: 42

    Skin.SectionBackground {
        anchors.fill: parent
        radius: 0
    }
    // Strong rules on both edges: the bar is the seam between the deck/mixer
    // controls above and the library below, so the hairline the panel frame
    // would give it is not enough to read as a section break.
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        color: Theme.borderStrong
        height: 2
        z: 2
    }
    Rectangle {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        color: Theme.borderStrong
        height: 2
        z: 2
    }
    Mixxx.ControlProxy {
        id: boothEnabledControl

        group: "[Master]"
        key: "booth_enabled"
    }

    // Output knobs, far left.
    Row {
        id: leftControls

        anchors.left: parent.left
        anchors.leftMargin: root.sideMargin
        anchors.verticalCenter: parent.verticalCenter
        spacing: Theme.space2

        OutputTile {
            group: "[Master]"
            height: 38
            key: "balance"
            label: qsTr("BAL")
            readoutMode: "balance"
            width: root.tileWidth
        }
        OutputTile {
            enabled: boothEnabledControl.value > 0
            group: "[Master]"
            height: 38
            key: "booth_gain"
            label: qsTr("BOOTH")
            opacity: enabled ? 1 : 0.45
            readoutMode: "gain"
            width: root.tileWidth
        }
        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            color: Theme.border
            height: 36
            width: 1
        }
        OutputTile {
            group: "[Master]"
            height: 38
            key: "headGain"
            label: qsTr("HEAD")
            readoutMode: "gain"
            width: root.tileWidth
        }
        OutputTile {
            group: "[Master]"
            height: 38
            key: "headMix"
            label: qsTr("MIX")
            readoutMode: "mix"
            width: root.tileWidth
        }
    }

    // Crossfader, centered in the bar.
    CrossfaderUnit {
        id: crossfaderUnit

        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        // Was 44, which left the fader well taller than it needed to be; the
        // 2 x 12 px assignment keys now set the height.
        height: 34
        show4decks: root.show4decks
        // Centered, so the free half-width is limited by whichever side group
        // reaches closer to the middle; otherwise the keys sit under the tiles.
        width: Math.max(160, Math.min(360, 2 * (Math.min(root.width / 2 - leftControls.x - leftControls.width, monoButtons.x - root.width / 2) - root.sideMargin)))
    }

    // Compact master level meter. The toolbar meter hides below
    // Theme.masterMeterMinWidth, so the output bar takes over there. Two 8 px
    // rows keep it inside the bar's 38 px content height; the width is capped
    // to the gap beside the crossfader so it can never run under the mono keys.
    Row {
        id: compactMasterMeter

        readonly property int meterWidth: Math.max(0, Math.min(150, monoButtons.x - x - Theme.space2))

        Accessible.description: qsTr("Left %1 percent, right %2 percent").arg(Math.round(compactMeterLeft.level * 100)).arg(Math.round(compactMeterRight.level * 100))
        Accessible.name: qsTr("Master output level")
        Accessible.role: Accessible.ProgressBar
        anchors.verticalCenter: parent.verticalCenter
        spacing: Theme.space1
        visible: root.width < Theme.masterMeterMinWidth && meterWidth >= 48
        width: meterWidth
        x: crossfaderUnit.x + crossfaderUnit.width + Theme.space2

        Text {
            id: compactMeterLegend

            anchors.verticalCenter: parent.verticalCenter
            color: Theme.textMuted
            font.capitalization: Font.AllUppercase
            font.family: Theme.fontFamily
            font.letterSpacing: Theme.letterSpacingCaps
            font.pixelSize: Theme.fontSizeXs - 1
            font.weight: Theme.fontWeightDemiBold
            text: qsTr("Out")
        }
        Column {
            anchors.verticalCenter: parent.verticalCenter
            spacing: 3
            width: compactMasterMeter.width - compactMeterLegend.width - compactMasterMeter.spacing

            Row {
                height: 8
                spacing: 3
                width: parent.width

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
                    id: compactMeterLeft

                    group: "[Main]"
                    height: 8
                    key: "vu_meter_left"
                    width: parent.width - 8 - 3
                }
            }
            Row {
                height: 8
                spacing: 3
                width: parent.width

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
                    id: compactMeterRight

                    group: "[Main]"
                    height: 8
                    key: "vu_meter_right"
                    width: parent.width - 8 - 3
                }
            }
        }
    }

    // Mono toggles, stacked to the left of the split key: master (and booth)
    // output, headphone cue mix, and the recording file. The keys are plain
    // gray/blue state squares with the caption beside them.
    Column {
        id: monoButtons

        anchors.right: splitButton.left
        anchors.rightMargin: Theme.space2
        anchors.verticalCenter: parent.verticalCenter
        // Three 12 px rows plus 1 px gaps fill the bar's 38 px content
        // height, so the recording mono key can join the two output keys
        // without growing the bar.
        spacing: 1

        MonoRow {
            accessibleName: qsTr("Mono master output")
            caption: qsTr("Out mono")
            controlGroup: "[Master]"
            controlKey: "mono_mixdown"
        }
        MonoRow {
            accessibleName: qsTr("Mono headphone cue")
            caption: qsTr("Cue mono")
            controlGroup: "[Master]"
            controlKey: "head_mono_mixdown"
        }
        MonoRow {
            accessibleName: qsTr("Record in mono")
            caption: qsTr("Rec mono")
            controlGroup: "[Recording]"
            controlKey: "mono_mixdown"
        }
    }

    // Headphone split toggle, pinned to the far right of the bar. The key is a
    // little smaller than the tiles so it keeps its clearance from the two
    // section rules now that they are 2 px thick.
    Skin.ControlButton {
        id: splitButton

        anchors.right: parent.right
        anchors.rightMargin: root.sideMargin
        anchors.verticalCenter: parent.verticalCenter
        group: "[Master]"
        height: 34
        key: "headSplit"
        text: qsTr("Split")
        toggleable: true
        width: root.tileWidth - 6
    }

    /// One mono toggle with its caption: a compact state square plus a
    /// micro-type label.
    component MonoRow: Row {
        id: monoRow

        required property string accessibleName
        required property string caption
        required property string controlGroup
        required property string controlKey

        height: 12
        spacing: Theme.space1

        // Round state lamp rather than a key: dark ring when off, accent filled
        // when the fold is engaged.
        Rectangle {
            id: lamp

            readonly property bool active: lampControl.value > 0

            function toggle() {
                lampControl.value = lamp.active ? 0 : 1;
            }

            Accessible.name: monoRow.accessibleName
            Accessible.role: Accessible.Button
            border.color: lamp.active ? Qt.lighter(Theme.accent, 1.45) : lampMouse.containsMouse ? Theme.borderStrong : Theme.border
            border.width: 1
            height: 8
            radius: width / 2
            width: 8
            y: 2

            gradient: Gradient {
                GradientStop {
                    color: lamp.active ? Qt.lighter(Theme.accent, 1.4) : lampMouse.containsMouse ? Theme.surfaceHover : Theme.surface3
                    position: 0.0
                }
                GradientStop {
                    color: lamp.active ? Qt.darker(Theme.accent, 1.35) : Theme.surface0
                    position: 1.0
                }
            }

            Accessible.onPressAction: lamp.toggle()

            Mixxx.ControlProxy {
                id: lampControl

                group: monoRow.controlGroup
                key: monoRow.controlKey
            }
            MouseArea {
                id: lampMouse

                anchors.fill: parent
                hoverEnabled: true

                onClicked: lamp.toggle()
            }
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            color: Theme.textMuted
            font.capitalization: Font.AllUppercase
            font.family: Theme.fontFamily
            font.letterSpacing: Theme.letterSpacingCaps
            font.pixelSize: Theme.fontSizeXs - 1
            font.weight: Theme.fontWeightDemiBold
            text: monoRow.caption
        }
    }
}
