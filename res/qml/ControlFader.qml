import "." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.12

Skin.Fader {
    id: root

    required property string group
    required property string key

    /// Highest control parameter this fader may write. Defaults to the top of
    /// the fader range, so faders that never set it behave exactly as before.
    /// Set it where the control reaches unity below the top of its range (or
    /// where the UI must never ask for gain above unity), e.g. stem volume:
    /// writes are clamped, and the handle cannot be dragged past this value.
    property real maximum: root.to

    value: Math.min(control.parameter, root.maximum)

    onMoved: function (value) {
        control.parameter = Math.min(value, root.maximum);
    }

    Mixxx.ControlProxy {
        id: control

        group: root.group
        key: root.key
    }
    TapHandler {
        onDoubleTapped: control.reset()
    }
    TapHandler {
        acceptedButtons: Qt.RightButton

        onTapped: control.reset()
    }
}
