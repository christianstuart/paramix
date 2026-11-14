pragma ComponentBehavior: Bound

import Mixxx 1.0 as Mixxx
import QtQuick
import QtQuick.Controls
import "../Theme"

/// Preset menu for a single effect slot.
Menu {
    id: root

    property color backgroundColor: Theme.surface2
    property color borderColor: Theme.border
    property url checkedSource
    property color disabledTextColor: Theme.textMuted
    property string fontFamily: Theme.fontFamily
    property int fontPixelSize: Theme.fontSizeMd
    property color hoverColor: Qt.alpha(Theme.accent, 0.22)
    property color hoverTextColor: Theme.textPrimary
    property color separatorColor: Theme.border
    // Legacy aliases kept for the LateNightQML skin.
    property color separatorBottomColor: separatorColor
    property color separatorTopColor: separatorColor
    required property Mixxx.EffectSlotProxy slot
    property url submenuArrowSource
    property color textColor: Theme.textPrimary

    bottomPadding: Theme.space1
    enabled: slot.loaded
    leftPadding: Theme.space1
    rightPadding: Theme.space1
    title: slot.number + ": " + slot.effectName
    topPadding: Theme.space1
    width: 180

    background: Rectangle {
        border.color: root.borderColor
        border.width: 1
        color: root.backgroundColor
        radius: Theme.radiusMd
    }
    delegate: EffectPresetMenuItem {
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
        model: root.slot.parametersModel

        delegate: EffectPresetMenuItem {
            required property bool loaded
            required property string name
            required property string parameterId

            checkable: true
            checked: loaded
            checkedSource: root.checkedSource
            disabledTextColor: root.disabledTextColor
            fontFamily: root.fontFamily
            fontPixelSize: root.fontPixelSize
            hoverColor: root.hoverColor
            hoverTextColor: root.hoverTextColor
            submenuArrowSource: root.submenuArrowSource
            text: name
            textColor: root.textColor

            onTriggered: root.slot.setParameterVisible(parameterId, !loaded)
        }

        onObjectAdded: (index, object) => root.insertItem(index, object)
        onObjectRemoved: (index, object) => root.removeItem(object)
    }
    EffectPresetMenuSeparator {
        lineColor: root.separatorColor
    }
    EffectPresetMenuItem {
        checkedSource: root.checkedSource
        disabledTextColor: root.disabledTextColor
        fontFamily: root.fontFamily
        fontPixelSize: root.fontPixelSize
        hoverColor: root.hoverColor
        hoverTextColor: root.hoverTextColor
        submenuArrowSource: root.submenuArrowSource
        text: qsTr("Save snapshot")
        textColor: root.textColor

        onTriggered: root.slot.saveDefaultSnapshot()
    }
}
