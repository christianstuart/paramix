import "." as Skin
import QtQuick
import "Theme"

/// Preview deck placeholder: a slim header eyebrow over a flat, recessed
/// surface. The real preview waveform can replace the centered hint without
/// changing the surrounding chrome.
Rectangle {
    id: root

    border.color: Theme.border
    border.width: 1
    color: Theme.surface1
    radius: Theme.radiusMd

    Item {
        id: header

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 22

        Skin.SectionText {
            anchors.fill: parent
            anchors.leftMargin: Theme.space2
            anchors.rightMargin: Theme.space2
            horizontalAlignment: Text.AlignLeft
            text: qsTr("Preview")
        }
        Rectangle {
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            color: Theme.border
            height: 1
        }
    }
    Text {
        anchors.centerIn: parent
        color: Theme.textMuted
        elide: Text.ElideRight
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeSm
        horizontalAlignment: Text.AlignHCenter
        text: qsTr("Drop a track here to preview")
    }
}
