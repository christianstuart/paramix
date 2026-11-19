import "." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.15
import QtQuick.Layouts
import "Theme"

/// A labelled 0..1 Pad FX parameter slider bound to a [ChannelN_PadFX] key.
RowLayout {
    id: root

    required property string group
    required property string key
    required property string label

    Layout.fillWidth: true
    spacing: Theme.space1

    Text {
        Layout.preferredWidth: 64
        color: Theme.textSecondary
        elide: Text.ElideRight
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeXs
        text: root.label
        verticalAlignment: Text.AlignVCenter
    }
    Skin.Slider {
        id: slider

        Layout.fillWidth: true
        enabled: root.enabled
        value: control.parameter

        Connections {
            target: slider.slider

            function onMoved() {
                control.parameter = slider.slider.value;
            }
        }
    }
    Mixxx.ControlProxy {
        id: control

        group: root.group
        key: root.key

        function syncSlider() {
            if (!slider.slider.pressed) {
                slider.slider.value = control.parameter;
            }
        }
        Component.onCompleted: syncSlider()
        onValueChanged: syncSlider()
    }
}
