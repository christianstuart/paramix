import QtQuick
import "Theme"

/// Placeholder for the preview deck pane below the library source tree.
Rectangle {
    id: root

    border.color: Theme.border
    border.width: 1
    color: Theme.surface1
    radius: Theme.radiusMd

    Text {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: Theme.space2
        color: Theme.textSecondary
        font.capitalization: Font.AllUppercase
        font.family: Theme.fontFamily
        font.letterSpacing: Theme.letterSpacingCaps
        font.pixelSize: Theme.fontSizeXs
        font.weight: Theme.fontWeightDemiBold
        text: qsTr("Preview")
    }
    Text {
        anchors.centerIn: parent
        color: Theme.textMuted
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeSm
        text: qsTr("Preview deck placeholder")
    }
}
