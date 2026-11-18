import ".." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick
import QtQuick.Controls
import "../Theme"

/// Effect selector for one slot. Uses the skin combo box so it follows the
/// flat theme instead of the platform style.
Skin.ComboBox {
    id: root

    readonly property string observedEffectId: root.slot?.effectId ?? ""
    required property Mixxx.EffectSlotProxy slot

    function syncCurrentEffect() {
        if (!slot.effectId) {
            // Hide the model's "---" placeholder; an empty slot should read as
            // empty, not as a selected no-effect entry.
            currentIndex = -1;
            return;
        }
        for (let index = 0; index < count; ++index) {
            if (model.get(index).effectId === slot.effectId) {
                currentIndex = index;
                return;
            }
        }
        currentIndex = -1;
    }

    Accessible.name: qsTr("Effect")
    Accessible.role: Accessible.ComboBox
    ToolTip.delay: 500
    ToolTip.text: displayText
    ToolTip.visible: hovered
    implicitHeight: Theme.fxControlHeight
    model: Mixxx.EffectsManager.visibleEffectsModel
    // The compact strip leaves the trigger narrow; keep the list wide enough
    // to read the effect names.
    popupMaxItem: 8
    popupWidth: Math.max(width, Math.round(150 * Theme.uiScale))
    textLeftPadding: indicator.width + Theme.space1
    textRole: "display"

    Component.onCompleted: syncCurrentEffect()
    onActivated: index => {
        const effectId = model.get(index).effectId || "";
        if (slot.effectId !== effectId) {
            slot.effectId = effectId;
        }
    }
    onCountChanged: syncCurrentEffect()
    onObservedEffectIdChanged: syncCurrentEffect()
}
