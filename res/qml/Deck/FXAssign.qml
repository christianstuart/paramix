import ".." as Skin
import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts
import "../Theme"

/// Deck FX unit assignment buttons (FX1..FX4).
///
/// A 12 px eyebrow over one row of 24 px toggles. Inactive units share the
/// neutral surface treatment; assigned units fill with the accent token.
Item {
    id: root

    required property string group

    implicitHeight: 40
    implicitWidth: 135

    Text {
        id: heading

        // The deck slot is 30 px tall at the minimum deck height: keys only.
        visible: root.height >= 40
        anchors.left: parent.left
        anchors.top: parent.top
        color: Theme.textMuted
        font.capitalization: Font.AllUppercase
        font.family: Theme.fontFamily
        font.letterSpacing: Theme.letterSpacingCaps
        font.pixelSize: Theme.fontSizeXs
        text: qsTr("FX Assign")
    }
    RowLayout {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        spacing: Theme.space1

        Repeater {
            model: 4

            Skin.ControlButton {
                id: fx1AssignButton

                required property int index

                Layout.fillHeight: true
                Layout.fillWidth: true
                Layout.maximumWidth: 32
                Layout.minimumWidth: 22
                Layout.preferredHeight: 24
                activeColor: Theme.textOnAccent
                group: `[EffectRack1_EffectUnit${index + 1}]`
                implicitHeight: 24
                key: `group_${root.group}_enable`
                text: `${index + 1}`
                toggleable: true

                Accessible.name: qsTr("FX unit %1").arg(index + 1)
            }
        }
    }
}
