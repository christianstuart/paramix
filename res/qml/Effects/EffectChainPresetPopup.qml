pragma ComponentBehavior: Bound

import "." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick
import QtQuick.Controls
import "../Theme"

/// Preset menu for a whole effect chain.
Menu {
    id: root

    property color backgroundColor: Theme.surface2
    property color borderColor: Theme.border
    property url checkedSource
    property color disabledTextColor: Theme.textMuted
    property string fontFamily: Theme.fontFamily
    property int fontPixelSize: Theme.fontSizeMd
    property color hoverColor: Qt.alpha(Theme.accent, 0.12)
    property color hoverTextColor: Theme.textPrimary

    /// Index of the next real chain preset. The standard preset model keeps the
    /// internal empty "---" preset at index 0, so stepping must never land on
    /// it (that would clear the chain). -1 when there is nothing to step to.
    readonly property int nextPresetIndex: {
        const count = presetInstantiator.count;
        if (count <= 1) {
            return -1;
        }
        const next = root.unit.presetIndex + 1;
        return (next <= 0 || next >= count) ? 1 : next;
    }
    // Legacy aliases kept for the LateNightQML skin.
    property color separatorBottomColor: separatorColor
    property color separatorColor: Theme.border
    property color separatorTopColor: separatorColor
    required property Mixxx.EffectSlotProxy slot1
    required property Mixxx.EffectSlotProxy slot2
    required property Mixxx.EffectSlotProxy slot3
    property url submenuArrowSource
    property color textColor: Theme.textPrimary
    required property Mixxx.EffectUnitProxy unit

    bottomPadding: Theme.space1
    leftPadding: Theme.space1
    rightPadding: Theme.space1
    topPadding: Theme.space1
    width: 180

    background: Rectangle {
        border.color: root.borderColor
        border.width: 1
        color: root.backgroundColor
        radius: Theme.radiusMd
    }
    delegate: Skin.EffectPresetMenuItem {
        checkedSource: root.checkedSource
        disabledTextColor: root.disabledTextColor
        fontFamily: root.fontFamily
        fontPixelSize: root.fontPixelSize
        hoverColor: root.hoverColor
        hoverTextColor: root.hoverTextColor
        submenuArrowSource: root.submenuArrowSource
        textColor: root.textColor
    }

    Instantiator {
        id: presetInstantiator

        model: Mixxx.EffectsManager.standardChainPresetModel

        delegate: Skin.EffectPresetMenuItem {
            required property int index
            required property string name
            required property string presetDisplay
            required property string tooltip

            ToolTip.delay: 500
            ToolTip.text: tooltip
            ToolTip.visible: hovered && tooltip.length > 0
            checkedSource: root.checkedSource
            disabledTextColor: root.disabledTextColor
            fontFamily: root.fontFamily
            fontPixelSize: root.fontPixelSize
            hoverColor: root.hoverColor
            hoverTextColor: root.hoverTextColor
            submenuArrowSource: root.submenuArrowSource
            text: (root.unit.presetName === name ? "✓ " : "") + presetDisplay
            textColor: root.textColor

            onTriggered: root.unit.loadPreset(index)
        }

        onObjectAdded: (index, object) => root.insertItem(index, object)
        onObjectRemoved: (index, object) => root.removeItem(object)
    }
    Skin.EffectPresetMenuItem {
        checkedSource: root.checkedSource
        disabledTextColor: root.disabledTextColor
        enabled: root.nextPresetIndex > 0
        fontFamily: root.fontFamily
        fontPixelSize: root.fontPixelSize
        hoverColor: root.hoverColor
        hoverTextColor: root.hoverTextColor
        submenuArrowSource: root.submenuArrowSource
        text: qsTr("Next Preset")
        textColor: root.textColor

        onTriggered: root.unit.loadPreset(root.nextPresetIndex)
    }
    Skin.EffectPresetMenuSeparator {
        lineColor: root.separatorColor
    }
    Skin.EffectPresetMenuItem {
        checkedSource: root.checkedSource
        disabledTextColor: root.disabledTextColor
        enabled: root.unit.canUpdatePreset
        fontFamily: root.fontFamily
        fontPixelSize: root.fontPixelSize
        hoverColor: root.hoverColor
        hoverTextColor: root.hoverTextColor
        submenuArrowSource: root.submenuArrowSource
        text: qsTr("Update Preset")
        textColor: root.textColor
        visible: enabled

        onTriggered: root.unit.updatePreset()
    }
    Skin.EffectPresetMenuItem {
        checkedSource: root.checkedSource
        disabledTextColor: root.disabledTextColor
        enabled: root.unit.canRenamePreset
        fontFamily: root.fontFamily
        fontPixelSize: root.fontPixelSize
        hoverColor: root.hoverColor
        hoverTextColor: root.hoverTextColor
        submenuArrowSource: root.submenuArrowSource
        text: qsTr("Rename Preset")
        textColor: root.textColor
        visible: enabled

        onTriggered: root.unit.renamePreset()
    }
    Skin.EffectPresetMenuItem {
        checkedSource: root.checkedSource
        disabledTextColor: root.disabledTextColor
        fontFamily: root.fontFamily
        fontPixelSize: root.fontPixelSize
        hoverColor: root.hoverColor
        hoverTextColor: root.hoverTextColor
        submenuArrowSource: root.submenuArrowSource
        text: qsTr("Save As New Preset...")
        textColor: root.textColor

        onTriggered: root.unit.savePresetAs()
    }
    Skin.EffectPresetMenuSeparator {
        lineColor: root.separatorColor
    }
    Skin.EffectSlotPresetMenu {
        backgroundColor: root.backgroundColor
        borderColor: root.borderColor
        checkedSource: root.checkedSource
        disabledTextColor: root.disabledTextColor
        fontFamily: root.fontFamily
        fontPixelSize: root.fontPixelSize
        hoverColor: root.hoverColor
        hoverTextColor: root.hoverTextColor
        separatorColor: root.separatorColor
        slot: root.slot1
        submenuArrowSource: root.submenuArrowSource
        textColor: root.textColor
    }
    Skin.EffectSlotPresetMenu {
        backgroundColor: root.backgroundColor
        borderColor: root.borderColor
        checkedSource: root.checkedSource
        disabledTextColor: root.disabledTextColor
        fontFamily: root.fontFamily
        fontPixelSize: root.fontPixelSize
        hoverColor: root.hoverColor
        hoverTextColor: root.hoverTextColor
        separatorColor: root.separatorColor
        slot: root.slot2
        submenuArrowSource: root.submenuArrowSource
        textColor: root.textColor
    }
    Skin.EffectSlotPresetMenu {
        backgroundColor: root.backgroundColor
        borderColor: root.borderColor
        checkedSource: root.checkedSource
        disabledTextColor: root.disabledTextColor
        fontFamily: root.fontFamily
        fontPixelSize: root.fontPixelSize
        hoverColor: root.hoverColor
        hoverTextColor: root.hoverTextColor
        separatorColor: root.separatorColor
        slot: root.slot3
        submenuArrowSource: root.submenuArrowSource
        textColor: root.textColor
    }
}
