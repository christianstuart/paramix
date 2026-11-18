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
        Stems = 3,
        PadFx = 4
    }

    /// 0 = move, 1 = cue, 2 = grid, 3 = stems, 4 = Pad FX. Seeded from `[Skin],deckN_pane`.
    property int activePane: Deck.Pane.Cue
    readonly property var currentTrack: deckPlayer.currentTrack
    property var deckPlayer: Mixxx.PlayerManager.getPlayer(group)
    required property string group
    property bool minimized: false
    /// Persisted per deck so the chosen pane survives a restart.
    readonly property string paneControlKey: "deck" + group.replace(/[^0-9]/g, "") + "_pane"
    /// Height shared by the MOVE / CUE / GRID / STEMS pane, the loop card and
    /// the tab stack: the pane sits beside the loop-size selector inside the
    /// transport block, so all three line up top and bottom. 112 px is the
    /// smallest height that keeps a wrapped 2 x 2 stem lane usable; the
    /// waveform row above absorbs whatever is left.
    readonly property int paneHeight: Theme.deckPaneHeight
    readonly property int paneTabsHeight: Theme.deckPaneHeight

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
                                    // the toolbar over the loop card, with the
                                    // MOVE/CUE/GRID selector pinned to the right
                                    // edge of that same row.
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
                                                    // The active pane sits beside the
                                                    // loop-size selector, inside the
                                                    // transport block: pane, loop card,
                                                    // then the MOVE/CUE/GRID tabs.
                                                    "type": "row",
                                                    "items": [
                                                        {
                                                            "type": "deckPane"
                                                        },
                                                        {
                                                            "type": "loop"
                                                        },
                                                        {
                                                            "type": "paneTabs"
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
    // 1 = cue, 2 = grid) that the skin control creator makes on first use, so
    // the choice survives a restart without touching C++. `persist` writes it
    // to the user config under [Skin].
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
            if (index >= Deck.Pane.Move && index <= Deck.Pane.Stems)
                root.activePane = index;
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
                // Fixed width card: the spacer before it absorbs the slack, so
                // the loop sits flush against the pane selector on the right.
                Layout.maximumWidth: 185
                Layout.minimumWidth: 120
                Layout.preferredHeight: 62
                Layout.preferredWidth: 185

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
                Layout.minimumHeight: root.paneHeight
                // 196, not 200: the tempo strip then still fits a 564 px deck,
                // which is what a 1280 px window (the layout floor) gives.
                Layout.minimumWidth: 196
                Layout.preferredHeight: root.paneHeight
                height: root.paneHeight
                objectName: "deckPane"

                // One stacked host keeps the pane band at a constant height, so
                // switching tabs never reflows the transport above it.
                //
                // The three panes live inside one wrapper item: the layout item
                // can only own a single content child, and stacking them here
                // keeps the host geometry independent of the active tab.
                Item {
                    id: paneStack

                    anchors.fill: parent

                    // The band itself. Each pane paints its own plate on top of
                    // this, but the host keeps a frame visible if a pane is ever
                    // hidden, so the strip cannot read as a hole in the deck.
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
                        paneMode: 1
                        // Cue is the fallback pane: it stays visible unless the
                        // selector explicitly asks for another tab, so the
                        // strip is never empty for an out-of-range selection.
                        visible: root.activePane !== Deck.Pane.Move && root.activePane !== Deck.Pane.Grid && root.activePane !== Deck.Pane.Stems && root.activePane !== Deck.Pane.PadFx

                        Skin.FadeBehavior on visible {
                            fadeTarget: cuePane
                        }
                    }
                    DeckComponent.HotcueAndStem {
                        id: stemsPane

                        anchors.fill: parent
                        currentTrack: root.currentTrack
                        group: root.group
                        paneMode: 2
                        visible: root.activePane === Deck.Pane.Stems

                        Skin.FadeBehavior on visible {
                            fadeTarget: stemsPane
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
                    DeckComponent.GridPane {
                        id: gridPane

                        anchors.fill: parent
                        group: root.group
                        visible: root.activePane === Deck.Pane.Grid

                        Skin.FadeBehavior on visible {
                            fadeTarget: gridPane
                        }
                    }
                }
            }
        }
        DelegateChoice {
            roleValue: "paneTabs"

            LayoutItem {
                Layout.alignment: Qt.AlignVCenter
                Layout.maximumHeight: root.paneTabsHeight
                Layout.maximumWidth: 34
                Layout.minimumHeight: root.paneTabsHeight
                Layout.minimumWidth: 34
                Layout.preferredHeight: root.paneTabsHeight
                Layout.preferredWidth: 34
                height: root.paneTabsHeight
                width: 34

                DeckComponent.DeckPaneTabs {
                    activePane: root.activePane
                    anchors.fill: parent
                    gridLocked: bpmlockControl.value > 0

                    onSelected: pane => root.showPane(pane)
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
