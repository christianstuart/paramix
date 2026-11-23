import ".." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick
import QtQuick.Shapes
import QtQuick.Layouts
import "../Theme"

/// Traktor-style beat size strip: halve chevron, as many power-of-two size
/// keys as fit (the current size lit), double chevron. Shared by the beatjump
/// pane and the loop row; the wheel scrolls the visible window.
Rectangle {
    id: root

    /// Accessible prefix, e.g. "Beatjump size".
    property string title: ""
    property var beatSizes: [1 / 32, 1 / 16, 1 / 8, 1 / 4, 1 / 2, 1, 2, 4, 8, 16, 32, 64]
    property string decrementKey: ""
    required property string group
    property string incrementKey: ""
    required property string key
    /// Upper bound on the size keys shown.
    property int maxKeys: 5
    /// Width each size key gets before another key is added.
    property int keyWidth: 34
    /// Keep the current size lit in the middle key: the chevrons and the
    /// wheel step the size and the strip scrolls with it.
    property bool centered: false

    readonly property color buttonColor: trackLoadedControl.value > 0 ? Theme.textSecondary : Theme.textMuted

    border.color: Theme.border
    border.width: 1
    color: Theme.surface2
    implicitWidth: 2 * 22 + root.keyWidth + 4 * Theme.space1
    radius: Theme.radiusSm

    Mixxx.ControlProxy {
        id: trackLoadedControl

        group: root.group
        key: "track_loaded"
    }
    // Rebuild the strip when the size changes from anywhere (keys, chevrons,
    // the size field, a controller). It must live outside the Repeater, whose
    // default property is its delegate.
    Connections {
        function onValueChanged() {
            sizeRepeater.selectedIndex = Math.max(0, sizeRepeater.values.indexOf(size.value));
            sizeRepeater.update();
        }

        target: size
    }
    BeatSizeSpinBoxBehavior {
        id: size

        beatSizes: root.beatSizes
        decrementKey: root.decrementKey
        group: root.group
        incrementKey: root.incrementKey
        key: root.key
    }
    RowLayout {
        id: strip

        anchors.fill: parent
        anchors.margins: Theme.space1
        spacing: Theme.space1

        WheelHandler {
            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
            onWheel: event => {
                if (root.centered) {
                    size.step(event.angleDelta.y > 0 ? 1 : -1);
                    return;
                }
                if (event.angleDelta.y < 0) {
                    sizeRepeater.adjustSelectedIndex(-1);
                } else if (event.angleDelta.y > 0) {
                    sizeRepeater.adjustSelectedIndex(1);
                }
            }
        }
        Chevron {
            Accessible.name: qsTr("Halve %1").arg(root.title)
            pointsLeft: true

            onPressed: size.step(-1)
        }
        Repeater {
            id: sizeRepeater

            property int selectedIndex: 0
            property int valueCount: Math.min(Math.max(1, Math.floor((strip.width - 2 * 22) / root.keyWidth)), root.maxKeys)
            property list<double> values: size.beatSizes

            function adjustSelectedIndex(delta) {
                sizeRepeater.selectedIndex = Math.min(Math.max(0, sizeRepeater.selectedIndex + delta), sizeRepeater.values.length - 1);
            }
            function update() {
                if (root.centered) {
                    // An odd window around the current size; slots past either
                    // end stay empty so the current size never leaves the middle.
                    const count = valueCount % 2 === 1 ? valueCount : Math.max(1, valueCount - 1);
                    const half = Math.floor(count / 2);
                    const current = size.nearestBeatSizeIndex(size.value);
                    let window = [];
                    for (let j = 0; j < count; ++j) {
                        const i = current + j - half;
                        window.push(i >= 0 && i < this.values.length ? this.values[i] : -1);
                    }
                    model = window;
                    return;
                }
                let values = [this.values[selectedIndex]];
                let appendMode = values[0] <= this.values[0];
                while (values.length < valueCount) {
                    if (appendMode) {
                        values.push(values[values.length - 1] * 2);
                    } else {
                        values = [values[0] / 2, ...values];
                    }
                    if (values[0] == this.values[0]) {
                        appendMode = true;
                    } else if (values[values.length - 1] == this.values[this.values.length - 1]) {
                        appendMode = false;
                    } else {
                        appendMode = !appendMode;
                    }
                }
                model = values;
            }

            Component.onCompleted: {
                update();
                selectedIndex = values.indexOf(size.value);
                if (selectedIndex < 0) {
                    selectedIndex = values.indexOf(4);
                }
            }
            onSelectedIndexChanged: update()
            onValueCountChanged: update()

            Skin.Button {
                property double currentSize: modelData
                required property int index
                required property var modelData

                Accessible.name: qsTr("%1 %2 beats").arg(root.title).arg(size.formatBeatSize(currentSize))
                Layout.fillHeight: true
                Layout.fillWidth: true
                Layout.minimumWidth: labelWidth
                activeColor: Theme.textOnAccent
                // Empty end slots of the centered strip keep their place.
                enabled: currentSize > 0
                // Centered: the middle key is the current size, always lit.
                highlight: root.centered
                        ? index === Math.floor(sizeRepeater.count / 2)
                        : currentSize == size.value && trackLoadedControl.value > 0
                implicitHeight: 18
                implicitWidth: root.keyWidth
                labelPadding: 2
                opacity: currentSize > 0 ? 1 : 0
                text: currentSize > 0 ? size.formatBeatSize(currentSize) : ""

                onPressed: size.commitText(currentSize.toString())
            }
        }
        Chevron {
            Accessible.name: qsTr("Double %1").arg(root.title)
            pointsLeft: false

            onPressed: size.step(1)
        }
    }

    /// Halve / double key with a small triangle.
    component Chevron: Skin.Button {
        id: chevron

        property bool pointsLeft: true

        Layout.fillHeight: true
        Layout.preferredWidth: 22
        implicitHeight: 18
        implicitWidth: 22

        contentItem: Item {
            anchors.fill: parent

            Shape {
                anchors.centerIn: parent
                antialiasing: true
                height: 10
                layer.enabled: true
                layer.samples: 4
                width: 10

                ShapePath {
                    fillColor: chevron.hovered ? Theme.textPrimary : root.buttonColor
                    startX: chevron.pointsLeft ? 0 : 10
                    startY: 5
                    strokeColor: 'transparent'

                    PathLine {
                        x: chevron.pointsLeft ? 10 : 0
                        y: 0
                    }
                    PathLine {
                        x: chevron.pointsLeft ? 10 : 0
                        y: 10
                    }
                    PathLine {
                        x: chevron.pointsLeft ? 0 : 10
                        y: 5
                    }
                }
            }
        }
    }
}
