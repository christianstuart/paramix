import "." as Skin
import Mixxx 1.0 as Mixxx
import Mixxx.Controls 1.0 as MixxxControls
import QtQuick 2.12
import QtQuick.Layouts
import QtQuick.Controls 2.12
import QtQml.Models // DelegateChoice for Qt >= 6.9
import Qt.labs.qmlmodels // DelegateChooser
import "Theme"

import "Deck" as DeckComponent

Item {
    id: root

    enum Pane {
        Move = 0,
        Cue = 1,
        Grid = 2,
        // 3 belonged to a removed stem pane and must stay unused.
        PadFx = 4
    }

    /// 0 = move, 1 = cue, 2 = grid, 4 = Pad FX. Seeded from
    /// `[Skin],deckN_pane`.
    property int activePane: Deck.Pane.Cue
    readonly property var currentTrack: deckPlayer.currentTrack
    property var deckPlayer: Mixxx.PlayerManager.getPlayer(group)
    required property string group
    property bool minimized: false
    /// Persisted per deck so the chosen pane survives a restart.
    readonly property string paneControlKey: "deck" + group.replace(/[^0-9]/g, "") + "_pane"
    /// Height shared by the MOVE / CUE / GRID / PAD FX pane band and the
    /// full-height loop card inside the transport block, so both line up top and
    /// bottom.
    readonly property int paneHeight: Theme.deckPaneHeight

    signal toggleFocus

    /// Whether a deck row item with the given width gate is shown.
    function rowItemShown(type, minWidth) {
        const jog = Mixxx.Config.paramixShowJogWheels;
        if (type === "spinny") {
            return jog;
        }
        if (root.width <= 0) {
            return true;
        }
        // Room for the jog wheel column or the tempo strip, not both.
        if (jog && root.width > Theme.deckSpinnyMinWidth && root.width <= Theme.deckFullRowMinWidth) {
            if (type === "tempo") {
                return false;
            }
            if (minWidth === Theme.deckFullRowMinWidth) {
                return true;
            }
        }
        return minWidth < root.width;
    }
    function showPane(pane) {
        root.activePane = pane;
        if (paneControl.initialized)
            paneControl.value = pane;
    }

    Drag.active: dragArea.drag.active
    Drag.dragType: Drag.Automatic
    Drag.mimeData: {
        let data = {
            "mixxx/player": group
        };
        const trackLocationUrl = currentTrack.trackLocationUrl;
        if (trackLocationUrl)
            data["text/uri-list"] = trackLocationUrl;

        return data;
    }
    Drag.supportedActions: Qt.CopyAction

    // Nothing may paint outside the deck card (the layout can overflow its
    // bounds when the window is short).
    clip: true

    Component.onCompleted: {
        itemModel.append({
            "type": "column",
            "items": [
                {
                    "type": "info"
                },
                {
                    "type": "row",
                    "items": [
                        {
                            "type": "column",
                            "items": [
                                {
                                    "type": "waveformOverview"
                                },
                                {
                                    // Transport row: play/cue on the left, then
                                    // the full-height loop card and the pane
                                    // column with its horizontal tabs above it.
                                    "type": "transport",
                                    "items": [
                                        {
                                            "type": "column",
                                            "items": [
                                                {
                                                    "type": "play"
                                                },
                                                {
                                                    "type": "cue"
                                                }
                                            ]
                                        },
                                        {
                                            "type": "column",
                                            "items": [
                                                {
                                                    "type": "toolbar"
                                                },
                                                {
                                                    "type": "vinyl"
                                                },
                                                {
                                                    // Loop card on the left, the pane
                                                    // column on the right: the
                                                    // CUE/MOVE/GRID/PAD FX row
                                                    // sits above the pane, so
                                                    // there is no vertical tab
                                                    // column.
                                                    "type": "row",
                                                    "items": [
                                                        {
                                                            "type": "loop"
                                                        },
                                                        {
                                                            "type": "deckPane"
                                                        }
                                                    ]
                                                }
                                            ]
                                        }
                                    ]
                                }
                            ]
                        },
                        {
                            "type": "column",
                            // The spinny needs the full deck minimum: main
                            // column, spinny/FX column and tempo strip. Below
                            // this the tempo fader is pushed over the mixer.
                            "minWidth": Theme.deckFullRowMinWidth,
                            "items": [
                                {
                                    "type": "spinny"
                                },
                                {
                                    "type": "fxAssign"
                                }
                            ]
                        },
                        {
                            "type": "tempo",
                            "minWidth": Theme.deckTempoMinWidth
                        }
                    ]
                }
            ]
        });
        minimizedItemModel.append({
            "type": "row",
            "items": [
                {
                    "type": "column",
                    "items": [
                        {
                            "type": "play"
                        },
                        {
                            "type": "cue"
                        }
                    ]
                },
                {
                    "type": "column",
                    "items": [
                        {
                            "type": "info"
                        },
                        {
                            "type": "waveformOverview"
                        }
                    ]
                }
            ]
        });
    }

    Skin.SectionBackground {
        anchors.fill: parent
        radius: Theme.radiusSm
    }
    Mixxx.ControlProxy {
        id: trackLoadedControl

        group: root.group
        key: "track_loaded"
    }
    Mixxx.ControlProxy {
        id: showVinylControl

        group: "[Skin]"
        key: "show_vinylcontrol"
    }

    // Persisted pane selection: a [Skin] ControlPushButton per deck (0 = move,
    // 1 = cue, 2 = grid, 4 = Pad FX) that the skin control creator makes on
    // first use, so the choice survives a restart without touching C++.
    // `persist` writes it to the user config under [Skin].
    Mixxx.SkinControlCreator {
        id: paneControlCreator

        defaultValue: Deck.Pane.Cue
        group: "[Skin]"
        key: root.paneControlKey
        persist: true
    }
    Mixxx.ControlProxy {
        id: paneControl

        group: "[Skin]"
        key: paneControlCreator.key

        onValueChanged: pane => {
            const index = Math.round(pane);
            if (index === Deck.Pane.Move || index === Deck.Pane.Cue
                    || index === Deck.Pane.Grid || index === Deck.Pane.PadFx) {
                root.activePane = index;
                return;
            }
            // A profile saved while the stem pane existed still stores 3. That
            // pane is gone, so open Cue and write the value back rather than
            // leaving the deck on a tab that no longer exists.
            root.activePane = Deck.Pane.Cue;
            if (paneControl.initialized && Math.round(paneControl.value) !== Deck.Pane.Cue)
                paneControl.value = Deck.Pane.Cue;
        }
    }
    Mixxx.ControlProxy {
        id: bpmlockControl

        group: root.group
        key: "bpmlock"
    }
    MouseArea {
        id: dragArea

        anchors.fill: root
        drag.target: root
    }
    ListModel {
        id: itemModel

        dynamicRoles: true
    }
    ListModel {
        id: minimizedItemModel
    }
    DelegateChooser {
        id: deckItemDelegate

        role: "type"

        DelegateChoice {
            roleValue: "info"

            LayoutItem {
                Layout.fillWidth: true
                Layout.preferredHeight: root.minimized ? 28 : Theme.deckHeaderHeight

                DeckComponent.InfoBar {
                    id: infoBar

                    anchors.fill: parent
                    group: root.group
                    minimized: root.minimized
                    rightColumnWidth: 105

                    TapHandler {
                        onDoubleTapped: root.toggleFocus()
                    }
                }
            }
        }
        DelegateChoice {
            roleValue: "loop"

            LayoutItem {
                Layout.alignment: Qt.AlignVCenter
                // The card fills the 112 px band and keeps the pane's 196 px
                // minimum beside it at the 564 px deck the 1280 window gives.
                // Min, max and preferred are all the band height so a layout
                // cannot stretch the card away from the pane beside it.
                Layout.maximumHeight: root.paneHeight
                Layout.maximumWidth: Theme.deckLoopCardWidth
                Layout.minimumHeight: root.paneHeight
                Layout.minimumWidth: Theme.deckLoopCardWidth
                Layout.preferredHeight: root.paneHeight
                Layout.preferredWidth: Theme.deckLoopCardWidth
                Layout.rightMargin: Theme.space1

                DeckComponent.Loop {
                    anchors.fill: parent
                    group: root.group
                }
            }
        }
        DelegateChoice {
            roleValue: "deckPane"

            LayoutItem {
                Layout.alignment: Qt.AlignVCenter
                Layout.fillWidth: true
                Layout.maximumHeight: root.paneHeight
                Layout.minimumHeight: root.paneHeight
                // 196, not 200: the loop card and the tempo strip still fit a
                // 564 px deck, which is what a 1280 px window gives.
                Layout.minimumWidth: Theme.deckPaneMinWidth
                Layout.preferredHeight: root.paneHeight

                ColumnLayout {
                    anchors.fill: parent
                    spacing: Theme.space1

                    // Horizontal CUE / MOVE / GRID / PAD FX tabs above the
                    // pane. The 16 px row replaces the old 34 px vertical
                    // column without changing the band height.
                    DeckComponent.DeckPaneTabs {
                        Layout.fillWidth: true
                        Layout.maximumHeight: Theme.deckPaneTabHeight
                        Layout.minimumHeight: Theme.deckPaneTabHeight
                        Layout.preferredHeight: Theme.deckPaneTabHeight
                        activePane: root.activePane
                        gridLocked: bpmlockControl.value > 0

                        onSelected: pane => root.showPane(pane)
                    }
                    // One stacked host keeps the pane band at a constant height,
                    // so switching tabs never reflows the transport above it.
                    //
                    // The four panes live inside one wrapper item: the layout
                    // item can only own a single content child, and stacking
                    // them here keeps the host geometry independent of the
                    // active tab.
                    Item {
                        id: paneStack

                        Layout.fillHeight: true
                        Layout.fillWidth: true
                        Layout.maximumHeight: Theme.deckPaneContentHeight
                        Layout.minimumHeight: Theme.deckPaneContentHeight
                        objectName: "deckPane"

                        // The pane plate. Each pane paints its own plate on top
                        // of this, but the host keeps a frame visible if a pane
                        // is ever hidden, so the strip cannot read as a hole in
                        // the deck.
                        Rectangle {
                            anchors.fill: parent
                            border.color: Theme.border
                            border.width: 1
                            color: Theme.surface1
                            radius: Theme.radiusMd
                        }
                        DeckComponent.BeatJump {
                            id: movePane

                            anchors.fill: parent
                            group: root.group
                            visible: root.activePane === Deck.Pane.Move

                            Skin.FadeBehavior on visible {
                                fadeTarget: movePane
                            }
                        }
                        DeckComponent.HotcueAndStem {
                            id: cuePane

                            anchors.fill: parent
                            currentTrack: root.currentTrack
                            group: root.group
                            // Cue is the fallback pane: it stays visible unless
                            // the selector explicitly asks for another tab, so
                            // the strip is never empty for an out-of-range
                            // selection.
                            visible: root.activePane !== Deck.Pane.Move && root.activePane !== Deck.Pane.Grid
                                     && root.activePane !== Deck.Pane.PadFx

                            Skin.FadeBehavior on visible {
                                fadeTarget: cuePane
                            }
                        }
                        DeckComponent.GridPane {
                            id: gridPane

                            anchors.fill: parent
                            group: root.group
                            visible: root.activePane === Deck.Pane.Grid

                            Skin.FadeBehavior on visible {
                                fadeTarget: gridPane
                            }
                        }
                        DeckComponent.PadFxPane {
                            id: padFxPane

                            anchors.fill: parent
                            group: root.group
                            visible: root.activePane === Deck.Pane.PadFx

                            Skin.FadeBehavior on visible {
                                fadeTarget: padFxPane
                            }
                        }
                    }
                }
            }
        }
        DelegateChoice {
            roleValue: "waveformOverview"

            LayoutItem {
                Layout.fillHeight: true
                Layout.fillWidth: true

                DeckComponent.WaveformOverview {
                    anchors.fill: parent
                    currentTrack: root.currentTrack
                    group: root.group
                }
            }
        }
        DelegateChoice {
            roleValue: "fxAssign"

            LayoutItem {
                id: holder

                required property int index

                Layout.alignment: Qt.AlignHCenter
                Layout.fillHeight: true
                objectName: "deckFxAssign"
                width: 135

                DeckComponent.FXAssign {
                    anchors.fill: parent
                    group: root.group
                }
            }
        }
        DelegateChoice {
            roleValue: "tempo"

            LayoutItem {
                Layout.fillHeight: true
                Layout.maximumWidth: 60
                Layout.minimumWidth: 60
                Layout.preferredWidth: 60

                DeckComponent.TempoColumn {
                    anchors.fill: parent
                    currentTrack: root.currentTrack
                    group: root.group
                }
            }
        }
        DelegateChoice {
            roleValue: "play"

            LayoutItem {
                Layout.preferredHeight: root.minimized ? 32 : 60
                Layout.preferredWidth: 60

                DeckComponent.PlayButton {
                    anchors.fill: parent
                    group: root.group
                    minimized: root.minimized
                }
            }
        }
        DelegateChoice {
            roleValue: "cue"

            LayoutItem {
                Layout.preferredHeight: root.minimized ? 32 : 60
                Layout.preferredWidth: 60

                DeckComponent.CueButton {
                    anchors.fill: parent
                    group: root.group
                    minimized: root.minimized
                }
            }
        }
        DelegateChoice {
            roleValue: "spinny"

            LayoutItem {
                Layout.alignment: Qt.AlignTop
                height: 140
                width: 140

                DeckComponent.Spinny {
                    anchors.fill: parent
                    group: root.group
                }
            }
        }
        DelegateChoice {
            roleValue: "row"

            LayoutContainer {
                Layout.fillHeight: true
                disposition: GridLayout.LeftToRight
            }
        }
        DelegateChoice {
            roleValue: "transport"

            // The transport row is content driven: play/cue next to the loop
            // card and the pane tabs. It must not grow, otherwise it steals
            // height from the waveform and pushes the pane off the deck.
            LayoutContainer {
                Layout.fillHeight: false
                Layout.fillWidth: true
                disposition: GridLayout.LeftToRight
            }
        }
        DelegateChoice {
            roleValue: "spacer"

            // Pushes the loop card and the pane selector to the right edge of
            // the transport row without adding a visible part.
            LayoutItem {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.preferredWidth: 0

                // A single, input-transparent filler keeps the layout item's
                // content slot populated without drawing anything.
                Item {
                    anchors.fill: parent
                }
            }
        }
        DelegateChoice {
            roleValue: "column"

            LayoutContainer {
                Layout.fillWidth: true
                disposition: GridLayout.TopToBottom
            }
        }
        DelegateChoice {
            roleValue: "toolbar"

            LayoutItem {
                Layout.fillWidth: true
                Layout.minimumHeight: 22
                Layout.preferredHeight: 22

                DeckComponent.Toolbar {
                    anchors.fill: parent
                    group: root.group
                }
            }
        }
        DelegateChoice {
            roleValue: "vinyl"

            LayoutItem {
                Layout.fillWidth: true
                Layout.minimumHeight: 20
                Layout.preferredHeight: 20
                // Collapse the row entirely so the deck keeps its height when
                // vinyl control is hidden or not built in.
                visible: showVinylControl.value > 0 && Mixxx.Application.vinylControlAvailable

                DeckComponent.VinylControlStrip {
                    anchors.fill: parent
                    group: root.group
                }
            }
        }
    }
    ColumnLayout {
        id: grid

        anchors.fill: parent
        anchors.margins: Theme.space1
        visible: !root.minimized

        FadeBehavior on visible {
            fadeTarget: grid
        }

        Repeater {
            delegate: deckItemDelegate
            model: itemModel
        }
    }
    ColumnLayout {
        id: minimizedGrid

        anchors.fill: parent
        anchors.margins: Theme.space1
        visible: root.minimized

        FadeBehavior on visible {
            fadeTarget: minimizedGrid
        }

        Repeater {
            delegate: deckItemDelegate
            model: minimizedItemModel
        }
    }
    Mixxx.PlayerDropArea {
        anchors.fill: parent
        group: root.group
    }

    component LayoutContainer: GridLayout {
        id: item

        property var disposition: GridLayout.TopToBottom

        columns: disposition == GridLayout.TopToBottom ? 1 : items.count
        visible: root.rowItemShown(typeof type === 'string' ? type : "", typeof minWidth === 'number' ? minWidth : 0)

        Repeater {
            delegate: deckItemDelegate
            model: parent.visible ? items : []
        }
    }
    component LayoutItem: Item {
        id: layoutItem

        default property var contentChildren

        visible: root.rowItemShown(typeof type === 'string' ? type : "", typeof minWidth === 'number' ? minWidth : 0)

        Loader {
            id: content

            active: layoutItem.visible
            anchors.fill: layoutItem

            sourceComponent: Component {
                Item {
                    children: layoutItem.contentChildren
                }
            }
        }
    }
}
