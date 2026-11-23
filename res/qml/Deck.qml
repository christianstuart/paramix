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
    /// The opt-in jog platter sits between the overview and the tempo strip,
    /// as tall as the top row.
    readonly property bool jogShown: Mixxx.Config.paramixShowJogWheels
            && (root.width <= 0 || root.width >= Theme.deckSpinnyMinWidth)

    signal toggleFocus

    /// Whether a deck row item with the given width gate is shown.
    function rowItemShown(type, minWidth) {
        if (type === "spinny") {
            return root.jogShown;
        }
        if (root.width <= 0) {
            return true;
        }
        if (type === "fxAssign") {
            return root.width >= Theme.deckFullRowMinWidth + 200;
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
                    // Controls on the left; the jog and the tempo strip run
                    // the full height on the right.
                    "type": "row",
                    "items": [
                        {
                            "type": "column",
                            "items": [
                                {
                                    "type": "waveformOverview"
                                },
                                {
                                    "type": "transport",
                                    "items": [
                                        {
                                            "type": "play"
                                        },
                                        {
                                            "type": "cue"
                                        },
                                        {
                                            "type": "toolbar"
                                        }
                                    ]
                                },
                                {
                                    "type": "transport",
                                    "items": [
                                        {
                                            "type": "loop"
                                        }
                                    ]
                                },
                                {
                                    "type": "vinyl"
                                },
                                {
                                    // The pane with its stacked selector.
                                    "type": "deckPane"
                                }
                            ]
                        },
                        {
                            "type": "spinny"
                        },
                        {
                            "type": "tempo"
                        },
                        {
                            "type": "fxAssign"
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

            // The loop row takes the transport row's spare width: its size
            // strip adds keys as the deck widens.
            LayoutItem {
                Layout.fillHeight: true
                Layout.fillWidth: true
                Layout.minimumWidth: loopRow.implicitWidth

                DeckComponent.Loop {
                    id: loopRow

                    anchors.fill: parent
                    group: root.group
                }
            }
        }
        DelegateChoice {
            roleValue: "deckPane"

            // Full-width pane row with the stacked CUE / MOVE / GRID / FX
            // selector on its right edge (Traktor style).
            LayoutItem {
                Layout.fillHeight: true
                Layout.fillWidth: true
                Layout.minimumHeight: Theme.deckPaneRowMinHeight

                RowLayout {
                    anchors.fill: parent
                    layoutDirection: Qt.RightToLeft
                    spacing: Theme.deckPaneTabGap

                    DeckComponent.DeckPaneTabs {
                        Layout.fillHeight: true
                        Layout.maximumWidth: Theme.deckPaneTabColumnWidth
                        Layout.minimumWidth: Theme.deckPaneTabColumnWidth
                        Layout.preferredWidth: Theme.deckPaneTabColumnWidth
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
                // Fixed height above the control rows; the minimized deck has
                // nothing else to give the height to.
                Layout.fillHeight: root.minimized
                Layout.fillWidth: true
                Layout.preferredHeight: root.minimized ? -1 : Theme.deckTopHeight

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

                Layout.fillHeight: true
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
                Layout.maximumWidth: Theme.deckTempoCompactWidth
                Layout.minimumWidth: Theme.deckTempoCompactWidth
                Layout.preferredWidth: Theme.deckTempoCompactWidth

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

                /// As wide as the deck row is tall up to deckJogMaxSize, capped so the controls keep
                /// their minimum width.
                readonly property real size: Math.max(0, Math.min(jogItem.height, Theme.deckJogMaxSize,
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

            // Fixed-height transport row: Play, Cue, deck modes, the loop row,
            // SYNC and range.
            LayoutContainer {
                Layout.fillHeight: false
                Layout.fillWidth: true
                Layout.maximumHeight: Theme.deckTransportRowHeight
                Layout.minimumHeight: Theme.deckTransportRowHeight
                Layout.preferredHeight: Theme.deckTransportRowHeight
                disposition: GridLayout.LeftToRight
            }
        }
        DelegateChoice {
            roleValue: "topRow"

            // Overview, optional jog and the compact tempo strip.
            LayoutContainer {
                Layout.fillHeight: false
                Layout.fillWidth: true
                Layout.maximumHeight: Theme.deckTopHeight
                Layout.minimumHeight: Theme.deckTopHeight
                Layout.preferredHeight: Theme.deckTopHeight
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
            roleValue: "toolbar"

            // Reverse, QNT, SLIP, RPT and eject, sized to their keys in the
            // transport row.
            LayoutItem {
                Layout.fillHeight: true
                Layout.minimumWidth: deckToolbar.implicitWidth
                Layout.preferredWidth: deckToolbar.implicitWidth

                DeckComponent.Toolbar {
                    id: deckToolbar

                    anchors.fill: parent
                    compactRow: true
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
