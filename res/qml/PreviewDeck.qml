import "." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick
import QtQuick.Layouts
import "Theme"

/// Preview deck: drop a track to audition it without loading a main deck.
///
/// The preview player is the engine's `[PreviewDeck1]` group, so cueing here
/// does not touch either deck's play state.
Rectangle {
    id: root

    readonly property var player: Mixxx.PlayerManager.getPlayer("[PreviewDeck1]")
    readonly property bool isLoaded: player?.trackLoaded === true

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
    Mixxx.PlayerDropArea {
        anchors.fill: parent
        group: "[PreviewDeck1]"
    }
    RowLayout {
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Theme.space2
        anchors.left: parent.left
        anchors.leftMargin: Theme.space2
        anchors.right: parent.right
        anchors.rightMargin: Theme.space2
        anchors.top: header.bottom
        anchors.topMargin: Theme.space1
        spacing: Theme.space2

        Text {
            Layout.fillHeight: true
            Layout.fillWidth: true
            color: root.isLoaded ? Theme.textPrimary : Theme.textMuted
            elide: Text.ElideRight
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeSm
            text: root.isLoaded
                  ? (root.player.currentTrack?.title ?? "")
                  : qsTr("Drop a track here to preview")
            verticalAlignment: Text.AlignVCenter
        }
        Skin.ControlButton {
            Layout.preferredHeight: 28
            Layout.preferredWidth: 56
            activeColor: Theme.textOnAccent
            enabled: root.isLoaded
            group: "[PreviewDeck1]"
            implicitHeight: 28
            key: "play_indicator"
            text: qsTr("Play")
            toggleable: true

            Accessible.name: qsTr("Preview play")
        }
    }
}
