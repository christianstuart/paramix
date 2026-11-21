import ".." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import "../Theme"

/// One labelled knob with a numeric readout, used by the mixer output strip.
///
/// The tile keeps the console micro-type legend above the knob and prints the
/// live control value underneath, so the main and headphone sections show the
/// same information a hardware master section would.
///
/// The host (Mixer.qml) assigns the width and never goes below 52 px, which is
/// the narrowest tile that still fits the widest caption ("BOOTH") and readout
/// ("CUE 50%") at the default font scale. Narrower columns wrap instead.
Item {
    id: root

    required property string group
    required property string key
    required property string label
    /// "gain" (dB), "balance" (L/C/R) or "mix" (cue/main split).
    property string readoutMode: "gain"

    readonly property real controlValue: readoutControl.value

    implicitHeight: 34
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
        anchors.fill: parent

        spacing: 0

        // The bar's top and bottom rules sit directly above and below the
        // tile; the caption, dial and readout use all 34 px between them.
        Skin.SectionText {
            color: Theme.textMuted
            height: 8
            text: root.label
            width: parent.width
        }
        Item {
            height: 1
            width: parent.width
        }
        Skin.ControlKnob {
            anchors.horizontalCenter: parent.horizontalCenter
            arcStart: root.readoutMode === "gain" ? Skin.Knob.ArcStart.Minimum : Skin.Knob.ArcStart.Center
            color: Theme.textPrimary
            group: root.group
            height: 16
            key: root.key
            width: 16
        }
        Text {
            color: Theme.textSecondary
            elide: Text.ElideRight
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeXs
            height: 9
            horizontalAlignment: Text.AlignHCenter
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
