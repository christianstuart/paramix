import ".." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.15
import QtQuick.Layouts
import "../Theme"

/// Deck hotcue grid, shown in the Cue pane.
///
/// The pane host keeps a constant height, so switching between Move, Cue and
/// Grid never reflows the transport above it.
Rectangle {
    id: root

    required property var currentTrack
    required property string group

    readonly property string emptyText: Mixxx.Config.paramixShowHotcues
            ? qsTr("Load a track to use hotcues")
            : qsTr("Hotcues are hidden in the interface settings")
    readonly property bool showHotcues: Mixxx.Config.paramixShowHotcues

    /// Choose dark or light text for a hotcue color so every pad stays legible.
    function hotcueTextColorFor(color) {
        if (!color || color.a <= 0)
            return Theme.textMuted;
        const luminance = 0.299 * color.r + 0.587 * color.g + 0.114 * color.b;
        return luminance > 0.6 ? "#08101C" : "#FFFFFF";
    }

    border.color: Theme.border
    border.width: 1
    // Nothing in a pane may paint over the loop card beside it.
    clip: true
    color: Theme.surface1
    radius: Theme.radiusMd

    Mixxx.ControlProxy {
        id: trackLoadedControl

        group: root.group
        key: "track_loaded"
    }

    // Empty state: without a track the tab body is hidden, so name what the
    // panel is for instead of leaving a blank plate. The host fills the whole
    // band, so the message stays centered.
    Item {
        id: emptyState

        anchors.fill: parent
        visible: trackLoadedControl.value == 0 || !root.showHotcues

        Text {
            anchors.centerIn: parent
            color: Theme.textMuted
            font.capitalization: Font.AllUppercase
            font.family: Theme.fontFamily
            font.letterSpacing: Theme.letterSpacingCaps
            font.pixelSize: Theme.fontSizeXs
            font.weight: Theme.fontWeightDemiBold
            horizontalAlignment: Text.AlignHCenter
            text: root.emptyText
            width: Math.max(80, Math.min(parent.width - 2 * Theme.space4, 260))
            wrapMode: Text.WordWrap
        }
    }
    Item {
        id: hotcueTab

        anchors.fill: parent
        visible: trackLoadedControl.value != 0 && root.showHotcues

        Skin.FadeBehavior on visible {
            fadeTarget: hotcueTab
        }

        GridLayout {
            anchors.bottomMargin: Theme.space2
            anchors.fill: parent
            anchors.leftMargin: Theme.space3
            anchors.rightMargin: Theme.space3
            anchors.topMargin: Theme.space2
            columnSpacing: Theme.space1
            rowSpacing: Theme.space1

            Repeater {
                model: 8

                // HotcueButton owns the press-to-activate and the right-click
                // color/clear popup; the face is redrawn here so a stored cue
                // color shows through and the pad keeps its number plus label.
                Skin.HotcueButton {
                    id: hotcueButton

                    required property int index
                    /// hotcuesModel holds only the set cues, and its
                    /// hotcueNumber is 0-based, so match on the number rather
                    /// than the row index.
                    readonly property string label: {
                        const model = root.currentTrack ? root.currentTrack.hotcuesModel : null;
                        if (!padCue.isSet || !model)
                            return "";
                        for (let row = 0; row < model.rowCount(); ++row) {
                            const cue = model.get(row);
                            if (cue && cue.hotcueNumber === hotcueButton.hotcueNumber - 1)
                                return cue.label ?? "";
                        }
                        return "";
                    }
                    /// Read-only view of the cue for the pad face; HotcueButton
                    /// keeps its own copy for the popup.
                    readonly property color textColor: padCue.isSet ? root.hotcueTextColorFor(padCue.color) : Theme.textMuted

                    Accessible.description: qsTr("Left click jumps to the hotcue, right click opens the color and clear menu")
                    Accessible.name: padCue.isSet ? qsTr("Hotcue %1").arg(index + 1) : qsTr("Hotcue %1, empty").arg(index + 1)
                    Layout.column: index % 4
                    Layout.fillHeight: true
                    Layout.fillWidth: true
                    Layout.row: parseInt(index / 4)
                    group: root.group
                    hotcueNumber: index + 1

                    background: Rectangle {
                        border.color: padCue.isSet ? Qt.lighter(padCue.color, 1.2) : Theme.border
                        border.width: 1
                        color: padCue.isSet ? padCue.color : Theme.surface1
                        radius: Theme.radiusSm

                        Behavior on color {
                            ColorAnimation {
                                duration: Theme.motionFast
                            }
                        }

                        // Lit top edge for the pad face.
                        Rectangle {
                            anchors.left: parent.left
                            anchors.leftMargin: 1
                            anchors.right: parent.right
                            anchors.rightMargin: 1
                            anchors.top: parent.top
                            anchors.topMargin: 1
                            color: padCue.isSet ? Qt.rgba(255, 255, 255, 0.28) : Theme.topHighlight
                            height: 1
                        }
                    }
                    contentItem: Item {
                        Column {
                            anchors.centerIn: parent
                            spacing: 0
                            width: parent.width

                            Text {
                                color: hotcueButton.textColor
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeLg
                                font.weight: Theme.fontWeightDemiBold
                                horizontalAlignment: Text.AlignHCenter
                                text: `${hotcueButton.hotcueNumber}`
                                width: parent.width
                            }
                            Text {
                                color: hotcueButton.textColor
                                elide: Text.ElideRight
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeXs
                                horizontalAlignment: Text.AlignHCenter
                                text: hotcueButton.label
                                visible: hotcueButton.label.length > 0
                                width: parent.width
                            }
                        }
                    }

                    Skin.Hotcue {
                        id: padCue

                        group: root.group
                        hotcueNumber: hotcueButton.hotcueNumber
                        useStoredColor: true
                    }
                }
            }
        }
    }
}
