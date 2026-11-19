pragma ComponentBehavior: Bound

import "." as Skin
import QtQuick 2.12
import QtQuick.Shapes 1.12
import QtQuick.Controls
import QtQuick.Layouts
import Mixxx 1.0 as Mixxx
import "Theme"

/// High / mid / low EQ plus the quick effect for a single channel.
///
/// Compact flat strip: a label under each knob, no tiles, tight spacing. Kill
/// state dims the knob; the filter knob carries the accent.
Column {
    id: root

    readonly property var bands: {
        const bands = [
            {
                "label": qsTr("HI"),
                "key": "parameter3",
                "statusKey": "button_parameter3"
            }
        ];
        if (root.fourBand) {
            bands.push({
                "label": qsTr("M-HI"),
                "key": "parameter4",
                "statusKey": "button_parameter4"
            });
        }
        bands.push({
            "label": qsTr("MID"),
            "key": "parameter2",
            "statusKey": "button_parameter2"
        }, {
            "label": qsTr("LOW"),
            "key": "parameter1",
            "statusKey": "button_parameter1"
        });
        return bands;
    }
    /// The fourth band exists only in the Biquad EQ (4 knobs); other mixing
    /// equalizers expose 3 (isolators) or a different set (graphic/parametric),
    /// so the extra knob stays hidden instead of controlling the wrong filter.
    readonly property bool fourBand: Mixxx.Config.paramixFourBandEq === true && eqParameterCount.value === 4
    required property string group

    spacing: 2

    Mixxx.ControlProxy {
        id: eqParameterCount

        group: "[EqualizerRack1_" + root.group + "_Effect1]"
        key: "num_parameters"
    }
    Repeater {
        model: root.bands

        Column {
            id: band

            required property var modelData

            spacing: 1
            width: root.width

            Skin.EqKnob {
                // 29 px box (27 px knob + 1 px padding) keeps the 2 px
                // inter-band gaps at fontScale 1.35 without growing the strip.
                height: 29
                knob.color: Theme.textPrimary
                knob.group: "[EqualizerRack1_" + root.group + "_Effect1]"
                knob.key: band.modelData.key
                statusKey: band.modelData.statusKey
            }
            EqLabel {
                text: band.modelData.label
                width: root.width
            }
        }
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
        currentIndex: fxSelect.value == -1 ? 0 : fxSelect.value
        // 36 px strip: drop the theme padding so the preset name has 26 px of
        // text instead of 16, elide rather than hard-cut the longer ones, and
        // let the height follow the type so fontScale 1.35 does not clip it.
        font.pixelSize: Theme.fontSizeSm
        implicitHeight: Math.max(16, Theme.fontSizeSm + 4)
        indicator.width: 0
        leftPadding: Theme.space1
        model: Mixxx.EffectsManager.quickChainPresetModel
        popupMaxItem: 8
        popupWidth: 100
        rightPadding: 0
        spacing: Theme.space1
        textLeftPadding: 0
        textRole: "display"
        // Leave 2 px on the right so the preset box does not read as touching
        // the fader in the next strip.
        width: root.width - 2

        // A two-word preset still cannot fit the 36 px strip; the list shows it
        // in full and hovering shows the whole name.
        ToolTip.delay: 500
        ToolTip.text: displayText
        ToolTip.visible: hovered

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

    component EqLabel: Text {
        color: Theme.textMuted
        elide: Text.ElideRight
        font.family: Theme.fontFamily
        font.letterSpacing: Theme.letterSpacingCaps
        font.pixelSize: Theme.fontSizeXs
        font.weight: Theme.fontWeightDemiBold
        horizontalAlignment: Text.AlignHCenter
    }
}
