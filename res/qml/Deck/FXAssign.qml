import ".." as Skin
import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts
import "../Theme"

/// Deck FX unit assignment buttons (FX1..FX4).
Item {
    id: root

    required property string group

    height: 35
    width: 135

    Text {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        color: Theme.textSecondary
        font.bold: true
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
                Layout.maximumWidth: 30
                Layout.minimumWidth: 22
                Layout.preferredHeight: 22
                activeColor: Theme.textOnAccent
                group: `[EffectRack1_EffectUnit${index + 1}]`
                implicitHeight: 22
                key: `group_${root.group}_enable`
                text: `${index + 1}`
                toggleable: true

                Accessible.name: qsTr("FX unit %1").arg(index + 1)
            }
        }
    }
}
