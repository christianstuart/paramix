import ".." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick
import QtQuick.Controls 2.15
import QtQuick.Layouts
import "../Theme"

/// Auto DJ controls above the queue.
///
/// Shown only while the Auto DJ source is active. Enable, fade, skip, shuffle
/// and random track are bound to the [AutoDJ] controls, so the bar and any
/// controller mapping drive the same state. Transition seconds and the repeat
/// preference use the same preferences as the classic DlgAutoDJ.
Rectangle {
    id: root

    readonly property bool hasQueueSelection: root.queueSelection ? root.queueSelection.hasSelection : false
    readonly property var queueSelection: {
        // TrackList.qml keeps its ItemSelectionModel private. Re-run the lookup
        // when the bar is shown and when the list's children change; the view
        // is the child that owns both our track list and a selection model.
        root.visible;
        const children = trackList ? trackList.data : null;
        if (!children) {
            return null;
        }
        for (let i = 0; i < children.length; ++i) {
            const child = children[i];
            if (child && child.model === root.sidebar.tracklist && child.selectionModel) {
                return child.selectionModel;
            }
        }
        return null;
    }

    /// The library sidebar proxy; supplies the state and the queue edits.
    required property var sidebar
    /// The track list that shows the queue. Its selection is the target of the
    /// remove and move buttons.
    required property var trackList

    function selectedQueueRows() {
        const selection = root.queueSelection;
        if (!selection) {
            return [];
        }
        const seen = {};
        const indexes = selection.selectedIndexes;
        for (let i = 0; i < indexes.length; ++i) {
            seen[indexes[i].row] = true;
        }
        return Object.keys(seen).map(row => Number(row)).sort((a, b) => a - b);
    }

    color: Theme.surface1
    implicitHeight: Math.round(32 * Theme.uiScale)

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        color: Theme.topHighlight
        height: 1
    }
    Rectangle {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        color: Theme.borderStrong
        height: 1
    }
    RowLayout {
        anchors.bottomMargin: Theme.space1
        anchors.fill: parent
        anchors.leftMargin: Theme.space2
        anchors.rightMargin: Theme.space2
        anchors.topMargin: Theme.space1
        spacing: Theme.space1

        Mixxx.ControlProxy {
            id: autoDjEnabled

            group: "[AutoDJ]"
            key: "enabled"
        }
        Mixxx.ControlProxy {
            id: autoDjFadeNow

            group: "[AutoDJ]"
            key: "fade_now"
        }
        Mixxx.ControlProxy {
            id: autoDjSkipNext

            group: "[AutoDJ]"
            key: "skip_next"
        }
        Mixxx.ControlProxy {
            id: autoDjShuffle

            group: "[AutoDJ]"
            key: "shuffle_playlist"
        }
        Mixxx.ControlProxy {
            id: autoDjAddRandom

            group: "[AutoDJ]"
            key: "add_random_track"
        }
        BarButton {
            Accessible.name: autoDjEnabled.value > 0 ? qsTr("Disable Auto DJ") : qsTr("Enable Auto DJ")
            ToolTip.delay: 500
            ToolTip.text: autoDjEnabled.value > 0 ? qsTr("Disable Auto DJ") : qsTr("Enable Auto DJ")
            ToolTip.visible: hovered
            highlight: autoDjEnabled.value > 0
            text: autoDjEnabled.value > 0 ? qsTr("Disable") : qsTr("Enable")

            onClicked: autoDjEnabled.value = autoDjEnabled.value > 0 ? 0 : 1
        }
        BarButton {
            ToolTip.delay: 500
            ToolTip.text: qsTr("Trigger the transition to the next track")
            ToolTip.visible: hovered
            enabled: autoDjEnabled.value > 0
            text: qsTr("Fade")

            onClicked: autoDjFadeNow.trigger()
        }
        BarButton {
            ToolTip.delay: 500
            ToolTip.text: qsTr("Skip the next track in the Auto DJ queue")
            ToolTip.visible: hovered
            enabled: autoDjEnabled.value > 0
            text: qsTr("Skip")

            onClicked: autoDjSkipNext.trigger()
        }
        Skin.TextField {
            Accessible.name: qsTr("Transition time in seconds")
            Layout.alignment: Qt.AlignVCenter
            Layout.preferredWidth: Math.round(46 * Theme.uiScale)
            ToolTip.delay: 500
            ToolTip.text: qsTr("Determines the duration of the transition")
            ToolTip.visible: hovered
            font.pixelSize: Theme.fontSizeSm
            horizontalAlignment: TextInput.AlignRight
            inputMethodHints: Qt.ImhFormattedNumbersOnly
            text: root.sidebar ? String(root.sidebar.autoDjTransitionSeconds) : ""

            validator: IntValidator {
                bottom: -99
                top: 99
            }

            // The binding is dropped while the user types, so restore it after
            // the value was committed to the preference.
            onEditingFinished: {
                const seconds = parseInt(text, 10);
                if (!isNaN(seconds)) {
                    root.sidebar.autoDjTransitionSeconds = Math.max(-99, Math.min(99, seconds));
                }
                text = Qt.binding(function () {
                    return root.sidebar ? String(root.sidebar.autoDjTransitionSeconds) : "";
                });
            }
        }
        Text {
            Accessible.ignored: true
            Layout.alignment: Qt.AlignVCenter
            color: Theme.textMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeSm
            text: qsTr("sec.")
        }
        BarButton {
            ToolTip.delay: 500
            ToolTip.text: qsTr("Shuffle the content of the Auto DJ queue")
            ToolTip.visible: hovered
            text: qsTr("Shuffle")

            onClicked: autoDjShuffle.trigger()
        }
        BarButton {
            ToolTip.delay: 500
            ToolTip.text: qsTr("Add a random track from the Auto DJ track sources to the queue")
            ToolTip.visible: hovered
            text: qsTr("Random")

            onClicked: autoDjAddRandom.trigger()
        }
        BarButton {
            ToolTip.delay: 500
            ToolTip.text: qsTr("Repeat the playlist")
            ToolTip.visible: hovered
            highlight: root.sidebar ? root.sidebar.autoDjRepeat : false
            text: qsTr("Repeat")

            onClicked: root.sidebar.autoDjRepeat = !root.sidebar.autoDjRepeat
        }
        Item {
            Layout.fillWidth: true
        }
        BarButton {
            ToolTip.delay: 500
            ToolTip.text: qsTr("Remove the selected tracks from the Auto DJ queue")
            ToolTip.visible: hovered
            enabled: root.hasQueueSelection
            text: qsTr("Remove")

            onClicked: root.sidebar.removeAutoDjTracks(root.selectedQueueRows())
        }
        BarButton {
            ToolTip.delay: 500
            ToolTip.text: qsTr("Move the selected tracks up in the queue")
            ToolTip.visible: hovered
            enabled: root.hasQueueSelection
            text: qsTr("Up")

            onClicked: root.sidebar.moveAutoDjTracks(root.selectedQueueRows(), -1)
        }
        BarButton {
            ToolTip.delay: 500
            ToolTip.text: qsTr("Move the selected tracks down in the queue")
            ToolTip.visible: hovered
            enabled: root.hasQueueSelection
            text: qsTr("Down")

            onClicked: root.sidebar.moveAutoDjTracks(root.selectedQueueRows(), 1)
        }
        BarButton {
            ToolTip.delay: 500
            ToolTip.text: qsTr("Remove all tracks from the Auto DJ queue")
            ToolTip.visible: hovered
            text: qsTr("Clear")

            onClicked: root.sidebar.clearAutoDjQueue()
        }
    }

    component BarButton: Skin.Button {
        Layout.alignment: Qt.AlignVCenter
    }
}
