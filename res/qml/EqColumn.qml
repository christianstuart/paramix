import "." as Skin
import QtQuick 2.12
import QtQuick.Shapes 1.12
import QtQuick.Layouts
import Mixxx 1.0 as Mixxx
import "Theme"

/// High / mid / low EQ plus the quick effect for a single channel.
///
/// Rekordbox channel EQ: labelled micro-type knobs over a recessed preset
/// well. Kill state dims the whole well; the filter knob carries the accent.
Column {
    id: root

    required property string group

    spacing: Theme.space1

    Text {
        color: Theme.textMuted
        elide: Text.ElideRight
        font.family: Theme.fontFamily
        font.letterSpacing: Theme.letterSpacingCaps
        font.pixelSize: Theme.fontSizeXs
        font.weight: Theme.fontWeightDemiBold
        horizontalAlignment: Text.AlignHCenter
        text: qsTr("HI")
        width: parent.width
    }

    Skin.EqKnob {
        knob.color: Theme.textPrimary
        knob.group: "[EqualizerRack1_" + root.group + "_Effect1]"
        knob.key: "parameter3"
        statusKey: "button_parameter3"
    }
    Text {
        color: Theme.textMuted
        elide: Text.ElideRight
        font.family: Theme.fontFamily
        font.letterSpacing: Theme.letterSpacingCaps
        font.pixelSize: Theme.fontSizeXs
        font.weight: Theme.fontWeightDemiBold
        horizontalAlignment: Text.AlignHCenter
        text: qsTr("MID")
        width: parent.width
    }
    Skin.EqKnob {
        knob.color: Theme.textPrimary
        knob.group: "[EqualizerRack1_" + root.group + "_Effect1]"
        knob.key: "parameter2"
        statusKey: "button_parameter2"
    }
    Text {
        color: Theme.textMuted
        elide: Text.ElideRight
        font.family: Theme.fontFamily
        font.letterSpacing: Theme.letterSpacingCaps
        font.pixelSize: Theme.fontSizeXs
        font.weight: Theme.fontWeightDemiBold
        horizontalAlignment: Text.AlignHCenter
        text: qsTr("LOW")
        width: parent.width
    }
    Skin.EqKnob {
        knob.color: Theme.textPrimary
        knob.group: "[EqualizerRack1_" + root.group + "_Effect1]"
        knob.key: "parameter1"
        statusKey: "button_parameter1"
    }
    // The quick-effect preset dropdown takes the place of the old "FILTER"
    // caption: it names the effect (which is usually the filter) and frees the
    // row the separate dropdown used to occupy, so the fader and the decks
    // above can be shorter.
    Mixxx.ControlProxy {
        id: fxSelect

        group: "[QuickEffectRack1_" + root.group + "]"
        key: "loaded_chain_preset"
    }
    Skin.ComboBox {
        id: effectSelector

        Accessible.name: qsTr("Quick effect preset")
        Accessible.role: Accessible.ComboBox
        clip: true
        currentIndex: fxSelect.value == -1 ? 0 : fxSelect.value
        font.pixelSize: Theme.fontSizeXs
        implicitHeight: 18
        indicator.width: 0
        model: Mixxx.EffectsManager.quickChainPresetModel
        popupMaxItem: 8
        popupWidth: 100
        rightPadding: Theme.space1
        spacing: Theme.space1
        textRole: "display"
        width: parent.width

        onActivated: index => {
            fxSelect.value = index;
        }
    }
    Skin.QuickFxKnob {
        group: "[QuickEffectRack1_" + root.group + "]"
        knob.arcStyle: ShapePath.DashLine
        knob.arcStylePattern: [2, 2]
        knob.color: Theme.accent
    }
}
