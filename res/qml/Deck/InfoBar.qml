import ".." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Shapes
import QtQuick.Layouts
import QtQml.Models // DelegateChoice for Qt >= 6.9
import Qt.labs.qmlmodels // DelegateChooser
import "../Theme"

/// Deck header: track identity, metadata and rating on a flat strip.
///
/// Traktor-style: slim deck-colour edge, tight bold title, uppercase
/// micro-labels for artist/meta. No card tinting.
Rectangle {
    id: root

    readonly property var currentTrack: deckPlayer?.currentTrack
    property var deckPlayer: Mixxx.PlayerManager.getPlayer(group)
    required property string group
    property color lineColor: Theme.border
    readonly property int metaSize: Theme.fontSizeXs
    property bool minimized: false
    required property int rightColumnWidth
    readonly property int secondarySize: root.minimized ? Theme.fontSizeXs : Theme.fontSizeSm

    // ── Density and artwork ────────────────────────────────────────────────
    readonly property bool showCoverArt: Mixxx.Config.paramixShowCoverArt !== false && !root.minimized
    readonly property int titleSize: root.minimized ? Theme.fontSizeSm : Theme.fontSizeLg + 1

    /// Valid track colour, or `null` when the track has none.
    readonly property var trackColorValue: {
        const value = root.currentTrack?.color;
        return value && value.valid ? value : null;
    }

    border.color: Theme.borderStrong
    border.width: 1
    color: Theme.surface2
    radius: Theme.radiusSm

    Rectangle {
        anchors.left: parent.left
        anchors.leftMargin: 1
        anchors.right: parent.right
        anchors.rightMargin: 1
        anchors.top: parent.top
        anchors.topMargin: 1
        color: {
            if (group === "[Channel1]")
                return Theme.deckA;
            if (group === "[Channel2]")
                return Theme.deckB;
            if (group === "[Channel3]")
                return Theme.deckC;
            return Theme.deckD;
        }
        height: 2
    }

    // Slim track-colour edge: 4 px strip, square, hardware-like.
    Rectangle {
        id: trackAccent

        anchors.bottom: parent.bottom
        anchors.bottomMargin: Theme.space2
        anchors.left: parent.left
        anchors.leftMargin: Theme.space1
        anchors.top: parent.top
        anchors.topMargin: Theme.space2
        color: root.trackColorValue ?? "transparent"
        radius: 0
        visible: root.showCoverArt
        width: 4
    }
    Item {
        id: coverArtFrame

        anchors.bottom: parent.bottom
        anchors.bottomMargin: root.minimized ? Theme.space1 : Theme.space2
        anchors.left: trackAccent.right
        anchors.leftMargin: Theme.space2
        anchors.top: parent.top
        anchors.topMargin: root.minimized ? Theme.space1 : Theme.space2 + 2
        clip: true
        visible: root.showCoverArt
        width: root.showCoverArt ? height : 0

        Rectangle {
            anchors.fill: parent
            color: Theme.surface3
            radius: Theme.radiusSm
        }
        Image {
            id: coverArt

            anchors.fill: parent
            asynchronous: true
            fillMode: Image.PreserveAspectCrop
            source: root.currentTrack?.coverArtUrl
            visible: status === Image.Ready
        }
        Rectangle {
            anchors.fill: parent
            border.color: Theme.borderStrong
            border.width: 1
            color: "transparent"
            radius: Theme.radiusSm
        }
    }
    DelegateChooser {
        id: cellDelegate

        role: "type"

        DelegateChoice {
            roleValue: "title"

            Cell {
                item.color: root.deckPlayer?.isLoaded ? Theme.textPrimary : Theme.textMuted
                item.font.family: Theme.fontFamilyDisplay
                item.font.letterSpacing: -0.4
                item.font.pixelSize: root.titleSize
                item.font.weight: root.deckPlayer?.isLoaded ? Theme.fontWeightDemiBold : Theme.fontWeightRegular
                item.text: root.deckPlayer?.isLoaded ? root.currentTrack?.title : qsTr("No track loaded")
                item.visible: true
            }
        }
        DelegateChoice {
            roleValue: "artist"

            Cell {
                item.color: Theme.textSecondary
                item.font.capitalization: Font.AllUppercase
                item.font.letterSpacing: Theme.letterSpacingCaps
                item.font.pixelSize: root.secondarySize
                item.text: root.currentTrack?.artist
            }
        }
        DelegateChoice {
            roleValue: "year"

            Cell {
                item.color: Theme.textMuted
                item.font.pixelSize: root.metaSize
                item.font.weight: Theme.fontWeightRegular
                item.text: root.currentTrack?.year
                visible: root.width > 500 && root.currentTrack?.year
            }
        }
        DelegateChoice {
            roleValue: "time"

            Cell {
                item.color: Theme.textPrimary
                item.font.family: Theme.fontFamilyMono
                item.font.pixelSize: root.minimized ? root.metaSize : Theme.fontSizeMd
                item.font.weight: Theme.fontWeightDemiBold
                item.horizontalAlignment: Text.AlignRight
                item.text: time.text
                visible: root.width > 450

                Mixxx.ControlProxy {
                    id: durationControl

                    group: root.group
                    key: "duration"
                }
                Mixxx.ControlProxy {
                    id: playPositionControl

                    group: root.group
                    key: "playposition"
                }
                TrackTime {
                    id: time

                    display: Mixxx.Config.controlPositionDisplay
                    elapsed: durationControl.value * playPositionControl.value
                    font.pixelSize: root.metaSize
                    mode: Mixxx.Config.controlTimeFormat
                    remaining: durationControl.value * (1 - playPositionControl.value)
                    visible: false
                }
            }
        }
        DelegateChoice {
            roleValue: "key"

            Cell {
                // Right-aligned so the key reads as part of the rating cluster
                // instead of floating in the middle of its fixed column.
                Layout.rightMargin: Theme.space2
                ToolTip.delay: 500
                ToolTip.text: qsTr("Sync and Reset Key")
                ToolTip.visible: keyMouse.containsMouse
                item.color: Theme.textPrimary
                item.font.family: Theme.fontFamilyMono
                item.font.pixelSize: root.secondarySize + 1
                item.font.weight: Theme.fontWeightDemiBold
                item.horizontalAlignment: Text.AlignRight
                item.text: Mixxx.KeyUtils.keyIsValid(visualKeyControl.value) ? Mixxx.KeyUtils.keyToString(visualKeyControl.value) : ""
                visible: root.width > 450

                Mixxx.ControlProxy {
                    id: visualKeyControl

                    group: root.group
                    key: "visual_key"
                }
                Mixxx.ControlProxy {
                    id: syncKeyControl

                    group: root.group
                    key: "sync_key"
                }
                Mixxx.ControlProxy {
                    id: resetKeyControl

                    group: root.group
                    key: "reset_key"
                }
                MouseArea {
                    id: keyMouse

                    Accessible.description: qsTr("Click to match the key of the other deck, right click to reset it to the track key")
                    Accessible.name: Mixxx.KeyUtils.keyIsValid(visualKeyControl.value) ? qsTr("Key %1").arg(Mixxx.KeyUtils.keyToString(visualKeyControl.value)) : qsTr("Key")
                    Accessible.role: Accessible.Button
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    activeFocusOnTab: true
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    enabled: root.deckPlayer?.isLoaded ?? false
                    hoverEnabled: true

                    Keys.onReturnPressed: syncKeyControl.trigger()
                    Keys.onSpacePressed: syncKeyControl.trigger()
                    onClicked: event => {
                        if (event.button === Qt.RightButton) {
                            resetKeyControl.trigger();
                        } else {
                            syncKeyControl.trigger();
                        }
                    }
                }
                Rectangle {
                    anchors.fill: parent
                    border.color: Theme.focusRing
                    border.width: 1
                    color: "transparent"
                    visible: keyMouse.activeFocus
                }
            }
        }
        DelegateChoice {
            roleValue: "rating"

            Item {
                id: cell

                required property int index
                property real ratio: ((rateRatioControl.value - 1) * 100).toPrecision(2)
                property bool showSeparator: index != parent.model.count - 1 && root.deckPlayer?.isLoaded

                Accessible.name: qsTr("Track rating")
                Accessible.role: Accessible.Button
                Layout.fillHeight: true
                Layout.fillWidth: index == 0
                Layout.preferredWidth: index == 0 ? 0 : stars.implicitWidth
                visible: root.width > 400

                Mixxx.ControlProxy {
                    id: rateRatioControl

                    group: root.group
                    key: "rate_ratio"
                }
                Row {
                    id: stars

                    anchors.centerIn: parent
                    spacing: 0
                    visible: root.deckPlayer?.isLoaded

                    Repeater {
                        model: 5

                        Shape {
                            id: star

                            antialiasing: true
                            height: 14
                            layer.enabled: true
                            layer.samples: 4
                            width: 16

                            ShapePath {
                                fillColor: {
                                    const isFilled = root.currentTrack?.stars > index;
                                    const rightPressed = mouse.containsMouse && (mouse.pressedButtons & Qt.RightButton);
                                    if (rightPressed && isFilled)
                                        return Theme.danger;
                                    if (mouse.containsMouse && !rightPressed && mouse.mouseX > star.x + stars.x)
                                        return Theme.accent;
                                    return isFilled ? Theme.warning : Theme.surface3;
                                }
                                startX: 8
                                startY: 0
                                strokeColor: 'transparent'

                                PathLine {
                                    x: 9.78701
                                    y: 5.18237
                                }
                                PathLine {
                                    x: 15.3496
                                    y: 5.18237
                                }
                                PathLine {
                                    x: 10.8494
                                    y: 8.38525
                                }
                                PathLine {
                                    x: 12.5683
                                    y: 13.5676
                                }
                                PathLine {
                                    x: 8.06808
                                    y: 10.3647
                                }
                                PathLine {
                                    x: 3.56787
                                    y: 13.5676
                                }
                                PathLine {
                                    x: 5.2868
                                    y: 8.38525
                                }
                                PathLine {
                                    x: 0.786587
                                    y: 5.18237
                                }
                                PathLine {
                                    x: 6.34915
                                    y: 5.18237
                                }
                                PathLine {
                                    x: 8.06808
                                    y: 0
                                }
                            }
                        }
                    }
                }
                MouseArea {
                    id: mouse

                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    anchors.fill: parent
                    hoverEnabled: true

                    onClicked: event => {
                        if (!root.currentTrack) {
                            return;
                        }
                        let selectedStars = Math.ceil((mouseX - stars.x) / 16);
                        if (event.button === Qt.RightButton) {
                            root.currentTrack.stars = 0;
                        } else if (selectedStars >= 0 && selectedStars <= 5) {
                            root.currentTrack.stars = selectedStars;
                        }
                    }
                }
            }
        }
        // The bottom row uses "none" as a gap filler between artist and rating;
        // without a choice for it the DelegateChooser logs a warning per cell.
        DelegateChoice {
            roleValue: "none"

            Item {
                Layout.fillHeight: true
                Layout.preferredWidth: 0
            }
        }
    }
    ListModel {
        id: topRowModel

        ListElement {
            type: "title"
        }
        ListElement {
            type: "year"
        }
        ListElement {
            type: "time"
        }
    }
    ListModel {
        id: bottomRowModel

        ListElement {
            type: "artist"
        }
        ListElement {
            type: "key"
        }
        ListElement {
            type: "rating"
        }
    }
    // The two text rows fill the header band; the old 20 px bottom inset was a
    // workaround for the shorter band and left the lower half empty.
    ColumnLayout {
        anchors.bottom: root.bottom
        anchors.bottomMargin: root.minimized ? 1 : 6
        anchors.left: coverArtFrame.visible ? coverArtFrame.right : root.left
        anchors.leftMargin: root.showCoverArt ? Theme.space3 : Theme.space2
        anchors.right: root.right
        anchors.rightMargin: Theme.space3
        anchors.top: root.top
        anchors.topMargin: root.minimized ? 0 : 6
        spacing: 0

        RowLayout {
            id: topRow

            readonly property bool isTop: true
            property var model: topRowModel

            // Both rows fill, so the height is shared in proportion to these.
            // Without them the title row took nearly everything and the row
            // below was crushed onto the header's bottom border.
            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.preferredHeight: 22

            Repeater {
                delegate: cellDelegate
                model: parent.model
            }
            // Tempo-master badge: neutral when another deck leads the sync,
            // accent blue when this deck is the one the others lock to.
            MasterPill {
                Layout.alignment: Qt.AlignVCenter
                Layout.leftMargin: Theme.space2
                group: root.group
                visible: !root.minimized
            }
        }
        RowLayout {
            id: bottomRow

            property var model: bottomRowModel

            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.preferredHeight: 18
            visible: !root.minimized

            Repeater {
                delegate: cellDelegate
                model: parent.model
            }
        }
    }

    component Cell: Item {
        id: cell

        required property int index
        readonly property bool isTop: !!parent.isTop
        property alias item: data
        property bool showSeparator: index != parent.model.count - 1 && root.deckPlayer?.isLoaded

        Layout.fillHeight: true
        Layout.fillWidth: index == 0
        Layout.preferredWidth: index == 0 ? 0 : rightColumnWidth

        Skin.EmbeddedText {
            id: data

            anchors.fill: parent
            color: isTop ? Theme.textPrimary : Theme.textSecondary
            elide: Text.ElideRight
            font.bold: false
            font.capitalization: Font.MixedCase
            font.family: Theme.fontFamily
            font.letterSpacing: isTop ? -0.5 : Theme.letterSpacingCaps
            font.pixelSize: isTop ? root.titleSize : root.secondarySize
            font.weight: isTop ? Theme.fontWeightBold : Theme.fontWeightMedium
            horizontalAlignment: index == 0 ? Text.AlignLeft : Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            visible: root.deckPlayer?.isLoaded

            Skin.FadeBehavior on visible {
                fadeTarget: data
            }
        }
    }

    /// Small status pill in the deck header: reads "MASTER" and turns accent
    /// while this deck is the sync leader (soft or explicit), i.e. the deck
    /// the other synced decks take their tempo from. Clicking it requests
    /// tempo leadership (sync_leader); pressing again does nothing, so a
    /// stray click cannot demote the deck.
    component MasterPill: Rectangle {
        id: pill

        required property string group
        readonly property bool isExplicitLeader: leaderValue >= 2
        readonly property bool isMaster: leaderValue > 0
        readonly property bool isSoftLeader: leaderValue === 1
        readonly property int leaderValue: Math.round(leaderControl.value)
        readonly property bool loaded: root.deckPlayer?.isLoaded ?? false

        function requestLeader() {
            if (!pill.loaded || pill.isMaster) {
                return;
            }
            leaderControl.value = 1;
        }

        Accessible.checkable: true
        Accessible.checked: pill.isMaster
        Accessible.description: pill.isMaster ? qsTr("This deck is the sync leader; the other decks take its tempo") : qsTr("Takes over tempo leadership from the other deck")
        Accessible.name: pill.isExplicitLeader ? qsTr("Tempo master, explicit leader") : pill.isSoftLeader ? qsTr("Tempo master, soft leader") : qsTr("Set as tempo master")
        Accessible.role: Accessible.Button
        ToolTip.delay: 500
        ToolTip.text: pill.isMaster ? qsTr("Tempo master") : qsTr("Set as tempo master")
        ToolTip.visible: pillMouse.containsMouse
        activeFocusOnTab: true
        border.color: pill.activeFocus ? Theme.focusRing : pill.isMaster ? Theme.accent : Theme.borderStrong
        border.width: 1
        color: pill.isMaster ? Theme.accent : "transparent"
        implicitHeight: 16
        implicitWidth: pillLabel.implicitWidth + 12
        radius: height / 2

        Behavior on color {
            ColorAnimation {
                duration: Theme.motionFast
            }
        }

        Keys.onReturnPressed: pill.requestLeader()
        Keys.onSpacePressed: pill.requestLeader()

        Text {
            id: pillLabel

            anchors.centerIn: parent
            color: pill.isMaster ? Theme.textOnAccent : Theme.textMuted
            font.capitalization: Font.AllUppercase
            font.family: Theme.fontFamily
            font.letterSpacing: Theme.letterSpacingCaps
            font.pixelSize: Theme.fontSizeXs
            font.weight: Theme.fontWeightDemiBold
            text: qsTr("Master")

            Behavior on color {
                ColorAnimation {
                    duration: Theme.motionFast
                }
            }
        }
        MouseArea {
            id: pillMouse

            acceptedButtons: Qt.LeftButton
            anchors.fill: parent
            cursorShape: pill.loaded && !pill.isMaster ? Qt.PointingHandCursor : Qt.ArrowCursor
            enabled: pill.loaded
            hoverEnabled: true

            onClicked: pill.requestLeader()
        }
        Mixxx.ControlProxy {
            id: leaderControl

            group: pill.group
            key: "sync_leader"
        }
    }
}
