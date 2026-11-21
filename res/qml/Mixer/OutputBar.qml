import ".." as Skin
import QtQuick 2.12
import "../Theme"

/// Headphone controls and the crossfader for the mixer.
///
/// The narrow mixer column cannot fit these without clipping, so they live in a
/// full-width bar directly below the deck/mixer row: the headphone gain and cue
/// mix on the far left and the crossfader centered in the bar.
Item {
    id: root

    readonly property int sideMargin: Theme.space3
    readonly property int tileWidth: root.width < 1200 ? 56 : 64

    // 24 px tiles and a 28 px fader unit between the two 2 px section rules.
    implicitHeight: 32

    Skin.SectionBackground {
        anchors.fill: parent
        radius: 0
    }
    // Strong rules on both edges: the bar is the seam between the deck/mixer
    // controls above and the library below, so the hairline the panel frame
    // would give it is not enough to read as a section break.
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        color: Theme.borderStrong
        height: 2
        z: 2
    }
    Rectangle {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        color: Theme.borderStrong
        height: 2
        z: 2
    }

    // Headphone gain and cue mix. Balance, booth, the mono folds and the split
    // key used to share this bar; they stay on controllers and in Preferences
    // so the bar can stay thin.
    Row {
        id: leftControls

        anchors.left: parent.left
        anchors.leftMargin: root.sideMargin
        anchors.verticalCenter: parent.verticalCenter
        spacing: Theme.space2

        OutputTile {
            accessibleName: qsTr("Headphone volume")
            group: "[Master]"
            headphoneIcon: true
            key: "headGain"
            label: qsTr("HEAD")
            readoutMode: "gain"
            width: root.tileWidth
        }
        OutputTile {
            accessibleName: qsTr("Headphone mix")
            group: "[Master]"
            key: "headMix"
            label: qsTr("MIX")
            readoutMode: "mix"
            width: root.tileWidth
        }
    }

    // Crossfader, centered in the bar. 360 px is the width it had when the
    // deck-assignment keys framed it; nothing else constrains it now.
    CrossfaderUnit {
        id: crossfaderUnit

        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        height: 28
        width: 360
    }
}
