import "." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts
import "Theme"

/// A labelled 0..1 Pad FX parameter slider bound to a [ChannelN_PadFX] key.
///
/// Only the parameter rows an effect actually uses are instantiated; the row
/// height comes from the Pad FX Theme tokens so every effect editor lines up.
RowLayout {
    id: root

    required property string group
    required property string key
    required property string label
    /// Optional owner name for assistive text, e.g. "Pad 2".
    property string ownerLabel: ""
    /// Label column width shared by every row of one editor, so all sliders
    /// start at the same x. 0 measures this row's own label.
    property int labelWidth: 0

    readonly property string accessibleName: root.ownerLabel.length > 0
            ? qsTr("%1 %2").arg(root.ownerLabel).arg(root.label)
            : root.label
    /// Sized from the text: a fixed token would cut "Wind-down" off.
    readonly property int labelColumnWidth: root.labelWidth > 0
            ? root.labelWidth
            : Math.ceil(labelMetrics.advanceWidth(root.label))

    FontMetrics {
        id: labelMetrics

        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeXs
    }

    Layout.fillWidth: true
    implicitHeight: Theme.padfxControlHeight
    spacing: Theme.space1

    Text {
        Layout.minimumWidth: root.labelColumnWidth
        Layout.preferredWidth: root.labelColumnWidth
        color: Theme.textSecondary
        elide: Text.ElideNone
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeXs
        text: root.label
        verticalAlignment: Text.AlignVCenter
    }
    Skin.Slider {
        id: slider

        Layout.fillWidth: true
        enabled: root.enabled
        height: Theme.padfxControlHeight
        value: control.parameter

        Accessible.description: qsTr("%1 for %2").arg(root.label).arg(root.ownerLabel)
        Accessible.name: root.accessibleName
        Accessible.role: Accessible.Slider

        ToolTip.delay: 500
        ToolTip.text: root.accessibleName
        ToolTip.visible: sliderHover.hovered

        HoverHandler {
            id: sliderHover
        }

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
