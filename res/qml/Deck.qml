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
    /// The opt-in jog platter sits beside the tempo strip once the deck has
    /// room for the whole row plus a usable platter.
    readonly property bool jogShown: Mixxx.Config.paramixShowJogWheels
            && (root.width <= 0 || root.width >= Theme.deckSpinnyMinWidth)

    signal toggleFocus

    /// Whether a deck row item with the given width gate is shown.
    function rowItemShown(type, minWidth) {
        if (type === "spinny") {
            return root.jogShown;
        }
        if (type === "sideColumn") {
            return true;
        }
        // The toolbar moves under the jog platter while the jog shows.
        if (type === "toolbar") {
            return !root.jogShown;
        }
        if (type === "toolbarSide") {
            return root.jogShown;
        }
        if (root.width <= 0) {
            return true;
        }
        if (type === "fxAssign") {
            return root.width >= Theme.deckFullRowMinWidth + (root.jogShown ? 140 : 0);
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
                            // Jog platter (opt-in) and the FX-assign keys,
                            // each with its own width gate in rowItemShown().
                            "type": "sideColumn",
                            "items": [
                                {
                                    "type": "spinny"
                                },
                                {
                                    "type": "toolbarSide"
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
                // The card and the pane beside it fill the band under the
                // fixed-height overview; deckLoopCardHeight is only the floor.
                Layout.fillHeight: true
                Layout.maximumWidth: Theme.deckLoopCardWidth
                Layout.minimumHeight: Theme.deckLoopCardHeight
                Layout.minimumWidth: Theme.deckLoopCardWidth
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
                Layout.fillHeight: true
                Layout.fillWidth: true
                Layout.minimumHeight: root.paneHeight
                // 196, not 200: the loop card and the tempo strip still fit a
                // 564 px deck, which is what a 1280 px window gives.
                Layout.minimumWidth: Theme.deckPaneMinWidth

                ColumnLayout {
                    anchors.fill: parent
                    spacing: Theme.deckPaneTabGap

                    // Horizontal CUE / MOVE / GRID / PAD FX tabs above the
                    // pane; tab row, gap and pane sum to the loop card height.
                    DeckComponent.DeckPaneTabs {
                        Layout.fillWidth: true
                        Layout.maximumHeight: Theme.deckPaneTabHeight
                        Layout.minimumHeight: Theme.deckPaneTabHeight
                        Layout.preferredHeight: Theme.deckPaneTabHeight
                        activePane: root.activePane
                        gridLocked: bpmlockControl.value > 0
                        group: root.group

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
                // Fixed height in the full deck so the cards get the rest; the
                // minimized deck has nothing else to give the height to.
                Layout.fillHeight: root.minimized
                Layout.fillWidth: true
                Layout.preferredHeight: root.minimized ? -1 : Theme.deckOverviewHeight

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
                // Under the jog the keys keep their 30 px slot; alone they fill.
                Layout.fillHeight: !root.jogShown
                Layout.preferredHeight: 30
                objectName: "deckFxAssign"
                // `required property index` hides the model's `type` role, so
                // name the gate explicitly.
                visible: root.rowItemShown("fxAssign", 0)
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
                // Play and Cue split the transport row's height, so they end
                // flush with the cards.
                Layout.fillHeight: !root.minimized
                Layout.preferredHeight: 32
                Layout.preferredWidth: Theme.deckTransportKeyWidth

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
                Layout.fillHeight: !root.minimized
                Layout.preferredHeight: 32
                Layout.preferredWidth: Theme.deckTransportKeyWidth

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
                id: jogItem

                /// As wide as the row is tall, capped so the cards keep their floor.
                readonly property real size: Math.max(0, Math.min(jogItem.height,
                        root.width - Theme.deckJogReservedWidth))

                Layout.fillHeight: true
                Layout.maximumWidth: jogItem.size
                Layout.minimumWidth: jogItem.size
                Layout.preferredWidth: jogItem.size

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

            // The transport row takes all height below the fixed overview, so
            // the cards grow instead of the waveform.
            LayoutContainer {
                Layout.fillHeight: true
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

            // Columns fill the row height, so the overview stretches and the
            // cards sit on the deck's bottom edge instead of floating.
            LayoutContainer {
                Layout.fillHeight: true
                Layout.fillWidth: true
                disposition: GridLayout.TopToBottom
            }
        }
        DelegateChoice {
            roleValue: "sideColumn"

            // Sized by its children (jog platter, FX keys), never stretched.
            LayoutContainer {
                Layout.fillHeight: true
                Layout.fillWidth: false
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
            roleValue: "toolbarSide"

            // The same toolbar, stacked in two rows under the jog platter.
            LayoutItem {
                Layout.fillWidth: true
                Layout.minimumHeight: Theme.deckSideToolbarHeight
                Layout.preferredHeight: Theme.deckSideToolbarHeight

                DeckComponent.Toolbar {
                    anchors.fill: parent
                    group: root.group
                    stacked: true
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
