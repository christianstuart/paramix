import ".." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import QtQuick.Shapes
import "../Theme"

/// One labelled knob with a numeric readout, used by the mixer output strip.
///
/// The label and readout sit beside the knob so the tile is only as tall as the
/// knob itself. The knob can be pinned to the tile's right edge to mirror the
/// tile across the bar, and the headphone gain shows a cue glyph above its knob
/// instead of the text label.
Item {
    id: root

    required property string group
    required property string key
    required property string label
    /// Name read out by assistive technology in place of the visual label.
    property string accessibleName: root.label
    /// Draw the headphone cue glyph above the knob instead of the text label.
    property bool headphoneIcon: false
    /// Mirror the tile: the knob moves to its right edge, the text to its left.
    property bool knobRight: false
    /// "gain" (dB), "balance" (L/C/R) or "mix" (cue/main split).
    property string readoutMode: "gain"

    readonly property real controlValue: readoutControl.value

    implicitHeight: 24
    implicitWidth: 54

    function formatReadout(value) {
        if (root.readoutMode === "balance") {
            if (Math.abs(value) < 0.01) {
                return qsTr("C");
            }
            return (value < 0 ? qsTr("L") : qsTr("R")) + Math.round(Math.abs(value) * 100);
        }
        if (root.readoutMode === "mix") {
            if (value <= -0.995) {
                return qsTr("CUE");
            }
            if (value >= 0.995) {
                return qsTr("MAIN");
            }
            return qsTr("CUE %1%").arg(Math.round((1 - value) * 50));
        }
        if (value <= 0.000001) {
            return qsTr("-\u221E dB");
        }
        const db = 20 * Math.log10(value);
        if (db < -60) {
            return qsTr("-\u221E dB");
        }
        return qsTr("%1 dB").arg((db > 0.05 ? "+" : "") + db.toFixed(1));
    }

    Column {
        id: knobColumn

        spacing: 0
        width: 16
        x: root.knobRight ? root.width - width : 0

        Item {
            height: 8
            width: 16

            visible: root.headphoneIcon

            // Cue glyph: the arc over two earcups.
            Shape {
                anchors.fill: parent

                ShapePath {
                    capStyle: ShapePath.RoundCap
                    fillColor: "transparent"
                    startX: 3
                    startY: 6
                    strokeColor: Theme.textMuted
                    strokeWidth: 1

                    PathArc {
                        direction: PathArc.Clockwise
                        radiusX: 5
                        radiusY: 5
                        useLargeArc: false
                        x: 13
                        y: 6
                    }
                }
            }
            Rectangle {
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.leftMargin: 2
                color: Theme.textMuted
                height: 4
                radius: 1
                width: 2
            }
            Rectangle {
                anchors.bottom: parent.bottom
                anchors.right: parent.right
                anchors.rightMargin: 2
                color: Theme.textMuted
                height: 4
                radius: 1
                width: 2
            }
        }
        Skin.ControlKnob {
            Accessible.name: root.accessibleName
            anchors.horizontalCenter: parent.horizontalCenter
            arcStart: root.readoutMode === "gain" ? Skin.Knob.ArcStart.Minimum : Skin.Knob.ArcStart.Center
            color: Theme.textPrimary
            group: root.group
            height: 16
            key: root.key
            width: 16
        }
    }
    Column {
        anchors.verticalCenter: parent.verticalCenter
        spacing: root.headphoneIcon ? 0 : 2
        width: root.width - 20
        x: root.knobRight ? 0 : 20

        Skin.SectionText {
            color: Theme.textMuted
            height: root.headphoneIcon ? 0 : 8
            horizontalAlignment: root.knobRight ? Text.AlignRight : Text.AlignLeft
            text: root.label
            visible: !root.headphoneIcon
            width: parent.width
        }
        Text {
            color: Theme.textSecondary
            elide: Text.ElideRight
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeXs
            height: 9
            horizontalAlignment: root.knobRight ? Text.AlignRight : Text.AlignLeft
            text: root.formatReadout(root.controlValue)
            verticalAlignment: Text.AlignVCenter
            width: parent.width
        }
    }
    Mixxx.ControlProxy {
        id: readoutControl

        group: root.group
        key: root.key
    }
}
