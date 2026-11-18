import ".." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import QtQuick.Layouts
import "../Deck" as DeckComponents
import "../Theme"

/// Compact preview player for the `[PreviewDeck1]` engine group.
///
/// The library preview buttons and the keyboard mapping (`p`, Shift+P,
/// Alt+Left/Right) drive the same group, so the bar reflects all of them and
/// reveals itself whenever preview playback starts. Closing the bar stops the
/// preview first, so no sound can keep playing behind a hidden bar.
///
/// shortcut: a maximized library hides this bar; `p` pressed there still plays
/// the preview unseen. Upgrade when the library layout reserves a preview row.
Item {
    id: root

    readonly property bool isLoaded: previewPlayer?.trackLoaded === true
    readonly property var previewPlayer: Mixxx.PlayerManager.getPlayer("[PreviewDeck1]")

    Accessible.name: qsTr("Preview deck")
    Accessible.role: Accessible.Grouping
    implicitHeight: 30

    Rectangle {
        anchors.fill: parent
        color: Theme.surface1
    }
    Rectangle {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        color: Theme.border
        height: 1
    }
    // Dropping a library track auditions it straight away. This is the only
    // load path that does not depend on a library selection.
    DropArea {
        anchors.fill: parent

        onDropped: drop => {
            if (!drop.hasUrls || drop.urls.length === 0 || !root.previewPlayer) {
                return;
            }
            root.previewPlayer.loadTrackFromLocationUrl(drop.urls[0], true);
            drop.accepted = true;
        }
    }
    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.space3
        anchors.rightMargin: Theme.space2
        spacing: Theme.space2

        Skin.SectionText {
            Layout.alignment: Qt.AlignVCenter
            Layout.preferredWidth: 48
            horizontalAlignment: Text.AlignLeft
            text: qsTr("Preview")
        }
        Text {
            Accessible.name: text
            Accessible.role: Accessible.StaticText
            Layout.alignment: Qt.AlignVCenter
            Layout.fillWidth: true
            Layout.minimumWidth: 40
            color: root.isLoaded ? Theme.textPrimary : Theme.textMuted
            elide: Text.ElideRight
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeSm
            text: root.isLoaded ? (root.previewPlayer.currentTrack?.title ?? "") : qsTr("Drop a track here to preview")
            verticalAlignment: Text.AlignVCenter
        }
        DeckComponents.TrackTime {
            Layout.alignment: Qt.AlignVCenter
            Layout.preferredWidth: 52
            color: Theme.textSecondary
            display: DeckComponents.TrackTime.Display.Elapsed
            elapsed: durationControl.value * playPositionControl.value
            font.pixelSize: Theme.fontSizeSm
            group: "[PreviewDeck1]"
            horizontalAlignment: Text.AlignRight
            remaining: durationControl.value * (1 - playPositionControl.value)
            visible: root.isLoaded
        }
        Skin.ControlButton {
            Accessible.name: qsTr("Load the selected track and play in the preview deck")
            Layout.alignment: Qt.AlignVCenter
            Layout.preferredHeight: 22
            Layout.preferredWidth: 50
            group: "[PreviewDeck1]"
            key: "LoadSelectedTrackAndPlay"
            text: qsTr("Load")

            // shortcut: the QML library wires LoadSelectedTrackAndPlay for decks
            // and samplers only, so this key is inert until Control.qml gains a
            // [PreviewDeck1] handler. Drop-to-preview works meanwhile.
        }
        Skin.ControlButton {
            Accessible.name: playIndicator.value > 0 ? qsTr("Pause preview") : qsTr("Play preview")
            Layout.alignment: Qt.AlignVCenter
            Layout.preferredHeight: 22
            Layout.preferredWidth: 54
            enabled: root.isLoaded
            group: "[PreviewDeck1]"
            key: "play"
            text: playIndicator.value > 0 ? qsTr("Pause") : qsTr("Play")
            toggleable: true
        }
        Skin.SectionText {
            Layout.alignment: Qt.AlignVCenter
            Layout.preferredWidth: 24
            horizontalAlignment: Text.AlignRight
            text: qsTr("Gain")
        }
        Skin.ControlKnob {
            Accessible.name: qsTr("Preview gain")
            Accessible.role: Accessible.Slider
            Layout.alignment: Qt.AlignVCenter
            Layout.preferredHeight: 20
            Layout.preferredWidth: 20
            color: Theme.textPrimary
            group: "[PreviewDeck1]"
            key: "pregain"
        }
        Skin.Button {
            Accessible.name: qsTr("Hide preview deck")
            Layout.alignment: Qt.AlignVCenter
            Layout.preferredHeight: 20
            Layout.preferredWidth: 20
            flat: true
            text: "\u00D7"

            onClicked: {
                previewPlay.value = 0;
                showPreviewDecks.value = 0;
            }
        }
    }
    Mixxx.ControlProxy {
        id: durationControl

        group: "[PreviewDeck1]"
        key: "duration"
    }
    Mixxx.ControlProxy {
        id: playPositionControl

        group: "[PreviewDeck1]"
        key: "playposition"
    }
    Mixxx.ControlProxy {
        id: playIndicator

        group: "[PreviewDeck1]"
        key: "play_indicator"

        // The keyboard and library preview paths all start playback here; make
        // sure the bar is on screen when they do.
        onValueChanged: {
            if (value > 0 && showPreviewDecks.value === 0) {
                showPreviewDecks.value = 1;
            }
        }
    }
    Mixxx.ControlProxy {
        id: previewPlay

        group: "[PreviewDeck1]"
        key: "play"
    }
    Mixxx.ControlProxy {
        id: showPreviewDecks

        group: "[Skin]"
        key: "show_preview_decks"
    }
}
