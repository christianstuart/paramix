import "." as Skin
import Mixxx 1.0 as Mixxx
import Mixxx.Controls 1.0 as MixxxControls
import Qt5Compat.GraphicalEffects
import QtQuick 2.12
import QtQuick.Layouts
import QtQuick.Shapes
import QtQuick.Controls 2.12
import QtQml.Models // DelegateChoice for Qt >= 6.9
import Qt.labs.qmlmodels // DelegateChooser
import "Theme"

import "Deck" as DeckComponent

Item {
    id: root

    enum Cardinality {
        North,
        East,
        South,
        West
    }

    enum Pane {
        Move = 0,
        Cue = 1,
        Grid = 2
    }

    readonly property var currentTrack: deckPlayer.currentTrack
    property var deckPlayer: Mixxx.PlayerManager.getPlayer(group)
    property bool editMode: false
    required property string group
    property bool minimized: false
    /// Height shared by the MOVE / CUE / GRID pane stack. The strip is constant
    /// so switching panes never reflows the transport above it, and it is a
    /// full-width band below that row, exactly like the reference. 112 px is
    /// the smallest height that keeps the 2 x 4 hotcue grid and the stem lanes
    /// usable; the waveform row above absorbs whatever is left.
    readonly property int paneHeight: Mixxx.Config.paramixCompactDecks ? 96 : 112
    /// The tab stack is the same height as the loop card so the two line up in
    /// the transport row and neither overlaps the other: 3 x 28 px tabs plus
    /// 2 x 4 px gaps.
    readonly property int paneTabsHeight: 92
    /// 0 = move, 1 = cue, 2 = grid. Seeded from `[Skin],deckN_pane`.
    property int activePane: Deck.Pane.Cue
    /// Persisted per deck so the chosen pane survives a restart.
    readonly property string paneControlKey: "deck" + group.replace(/[^0-9]/g, "") + "_pane"

    function showPane(pane) {
        root.activePane = pane;
        if (paneControl.initialized)
            paneControl.value = pane;
    }

    signal toggleFocus

    function serializeModel(item) {
        let result = new Array();
        for (let i = 0; i < item.count; i++) {
            let child = item.get(i);
            let object = {
                type: child.type
            };
            if (child.items !== undefined) {
                object.items = serializeModel(child.items);
            }
            result.push(object);
        }
        return result;
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
                                                    "type": "row",
                                                    "items": [
                                                        {
                                                            "type": "spacer"
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
                                },
                                {
                                    // Exactly one full-width strip directly
                                    // below the transport row; the tab stack
                                    // above it ends on the same right edge.
                                    "type": "deckPane"
                                }
                            ]
                        },
                        {
                            "type": "column",
                            // The spinny needs the full deck minimum: main
                            // column, spinny/FX column and tempo strip. Below
                            // this the tempo fader is pushed over the mixer.
                            "minWidth": 800,
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
                            "minWidth": 270
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
    Component.onDestruction: {
        // TODO to be saved by setting editor
        console.log(`normal interface for ${root.group}:`, JSON.stringify(serializeModel(itemModel)));
        console.log(`minimized interface for ${root.group}:`, JSON.stringify(serializeModel(minimizedItemModel)));
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
            if (index >= Deck.Pane.Move && index <= Deck.Pane.Grid)
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
                Layout.preferredHeight: root.minimized ? 28 : (Mixxx.Config.paramixCompactDecks ? 44 : Theme.deckHeaderHeight)
                editOverlay.visible: false

                DeckComponent.InfoBar {
                    id: infoBar

                    anchors.fill: parent
                    editMode: root.editMode
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
                // Fixed width card: the spacer before it absorbs the slack, so
                // the loop sits flush against the pane selector on the right.
                Layout.maximumHeight: 92
                Layout.maximumWidth: 185
                Layout.minimumWidth: 140
                Layout.preferredWidth: 185
                Layout.alignment: Qt.AlignVCenter
                editLabel.color: Theme.textPrimary
                editLabel.font.capitalization: Font.AllUppercase
                editLabel.text: qsTr("Loop")
                editOverlay.color: Theme.surface2
                height: 92

                DeckComponent.Loop {
                    anchors.fill: parent
                    group: root.group
                }
            }
        }
        DelegateChoice {
            roleValue: "deckPane"

            LayoutItem {
                Layout.fillWidth: true
                Layout.minimumHeight: root.paneHeight
                Layout.minimumWidth: 200
                Layout.preferredHeight: root.paneHeight
                Layout.alignment: Qt.AlignVCenter
                blurRadius: 4
                editLabel.color: Theme.textPrimary
                editLabel.font.pixelSize: Theme.fontSizeLg
                editLabel.text: qsTr("Move / Cue / Grid panes")
                editOverlay.color: Theme.scrim
                height: root.paneHeight

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
                        // Cue is the fallback pane: it stays visible unless the
                        // selector explicitly asks for another tab, so the
                        // strip is never empty for an out-of-range selection.
                        visible: root.activePane !== Deck.Pane.Move
                                 && root.activePane !== Deck.Pane.Grid

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
                blurRadius: 4
                editLabel.color: Theme.textPrimary
                editOverlay.color: Theme.surface2
                height: root.paneTabsHeight
                width: 34

                DeckComponent.DeckPaneTabs {
                    anchors.fill: parent
                    activePane: root.activePane
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
                blurRadius: 4
                editLabel.color: Theme.textPrimary
                editLabel.font.pixelSize: Theme.fontSizeLg
                editLabel.text: qsTr("Waveform overview")
                editOverlay.color: Theme.scrim

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
                editLabel.color: Theme.textSecondary
                editLabel.font.capitalization: Font.AllUppercase
                editLabel.text: qsTr("FX Assign")
                editOverlay.color: Theme.surface1
                width: 135

                DeckComponent.FXAssign {
                    group: root.group
                }
            }
        }
        DelegateChoice {
            roleValue: "tempo"

            LayoutItem {
                Layout.fillHeight: true
                Layout.maximumWidth: 75
                Layout.minimumWidth: 75
                Layout.preferredWidth: 75
                blurRadius: 4
                editLabel.color: Theme.textPrimary
                editLabel.text: qsTr("Tempo")
                editOverlay.color: Theme.scrim

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
                editLabel.color: Theme.textPrimary
                editLabel.horizontalAlignment: Text.AlignHCenter
                editLabel.text: qsTr("Play\nButton")
                editOverlay.border.color: Theme.surface0
                editOverlay.border.width: 1
                editOverlay.color: Theme.surface1

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
                editLabel.color: Theme.textPrimary
                editLabel.horizontalAlignment: Text.AlignHCenter
                editLabel.text: qsTr("Cue\nButton")
                editOverlay.border.color: Theme.surface0
                editOverlay.border.width: 1
                editOverlay.color: Theme.surface1

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
                editLabel.color: Theme.textSecondary
                editLabel.font.pixelSize: Theme.fontSizeLg
                editLabel.text: qsTr("Spinny")
                editOverlay.color: Theme.surface3
                editOverlay.radius: height
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
                editOverlay.visible: false

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
                Layout.minimumHeight: 24
                Layout.preferredHeight: 24
                blurRadius: 4
                editLabel.color: Theme.textPrimary
                editLabel.font.pixelSize: Theme.fontSizeLg
                editLabel.text: qsTr("Toolbar")
                editOverlay.color: Theme.scrim

                DeckComponent.Toolbar {
                    anchors.fill: parent
                    group: root.group
                }
            }
        }
    }
    ColumnLayout {
        id: grid

        anchors.fill: parent
        anchors.margins: Mixxx.Config.paramixCompactDecks ? 2 : Theme.space1
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
        anchors.margins: Mixxx.Config.paramixCompactDecks ? 2 : Theme.space1
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
        property var candidate: null

        function handleCardinalityEast(drag, modelRef, child, i, sourceIdx) {
            let spacing = drag.source.beginDrag.x - child.x - child.width;
            child.x += drag.source.width + spacing;
            drag.source.beginDrag.x -= child.width + spacing;

            if (drag.source.move) {
                drag.source.move.child = i;
                drag.source.move.count++;
            } else {
                drag.source.move = {
                    ref: modelRef,
                    source: sourceIdx,
                    target: i,
                    count: 1
                };
            }
        }
        function handleCardinalityNorth(drag, modelRef, child, i, sourceIdx) {
            let spacing = child.y - drag.source.beginDrag.y - drag.source.height;
            child.y = drag.source.beginDrag.y;
            drag.source.beginDrag.y += child.height + spacing;

            if (drag.source.move) {
                drag.source.move.source++;
            } else {
                drag.source.move = {
                    ref: modelRef,
                    source: i,
                    target: sourceIdx,
                    count: 1
                };
            }
        }
        function handleCardinalitySouth(drag, modelRef, child, i, sourceIdx) {
            let spacing = drag.source.beginDrag.y - child.y - child.height;
            drag.source.beginDrag.y = child.y;
            child.y += drag.source.height + spacing;

            if (drag.source.move) {
                drag.source.move.target = i;
                drag.source.move.count++;
            } else {
                drag.source.move = {
                    ref: modelRef,
                    source: sourceIdx,
                    target: i,
                    count: 1
                };
            }
        }
        function handleCardinalityWest(drag, modelRef, child, i, sourceIdx) {
            let spacing = child.x - drag.source.beginDrag.x - drag.source.width;
            child.x -= drag.source.width + spacing;
            drag.source.beginDrag.x += child.width + spacing;

            if (drag.source.move) {
                drag.source.move.source++;
            } else {
                drag.source.move = {
                    ref: modelRef,
                    source: i,
                    target: sourceIdx,
                    count: 1
                };
            }
        }
        function handleChild(i, child, sourceIdx, position) {
            let topParent = root.minimized ? minimizedGrid : grid;
            let itemArea = child.mapToItem(topParent, 0, 0, child.width, child.height);

            if (position.x >= itemArea.x && position.x < itemArea.x + itemArea.width && position.y >= itemArea.y && position.y < itemArea.y + itemArea.height) {
                let currentCenter = Qt.point(itemArea.x + itemArea.width / 2, itemArea.y + itemArea.height / 2);
                let sourceCenter = drag.source.mapToItem(topParent, drag.source.width / 2, drag.source.height / 2);

                if (candidate == null || candidate.target != child) {
                    let distance = Qt.point(Math.abs(sourceCenter.x - currentCenter.x), Math.abs(sourceCenter.y - currentCenter.y));

                    let cardinality;
                    if (distance.x >= distance.y) {
                        cardinality = sourceCenter.x > currentCenter.x ? Deck.Cardinality.East : Deck.Cardinality.West;
                    } else {
                        cardinality = sourceCenter.y > currentCenter.y ? Deck.Cardinality.South : Deck.Cardinality.North;
                    }
                    candidate = {
                        target: child,
                        cardinality: cardinality,
                        completed: false
                    };
                    return true;
                }

                if (candidate.completed) {
                    return true;
                }

                switch (candidate.cardinality) {
                case Deck.Cardinality.West:
                    candidate.completed = sourceCenter.x + drag.source.width / 2 > currentCenter.x;
                    break;
                case Deck.Cardinality.North:
                    candidate.completed = drag.source.y + drag.source.height > currentCenter.y - parent.height / 2;
                    break;
                case Deck.Cardinality.East:
                    candidate.completed = sourceCenter.x - drag.source.width / 2 < currentCenter.x;
                    break;
                case Deck.Cardinality.South:
                    candidate.completed = drag.source.y < currentCenter.y;
                    break;
                default:
                    console.error(`Unhandled cardinality value: '${candidate.cardinality}'`);
                    return true;
                }

                if (!candidate.completed) {
                    return true;
                }

                let delta = Qt.point(drag.source.beginDrag.x - child.x, drag.source.beginDrag.y - child.y);
                let currentParent = drag.source.parent;
                let reverse = [];
                while (currentParent != topParent) {
                    reverse.push(currentParent.index);
                    currentParent = currentParent.parent;
                }
                let modelRef = root.minimized ? minimizedItemModel : itemModel;
                while (reverse.length > 0) {
                    modelRef = modelRef.get(reverse.pop()).items;
                }

                switch (candidate.cardinality) {
                case Deck.Cardinality.West:
                    handleCardinalityWest(drag, modelRef, child, i, sourceIdx);
                    break;
                case Deck.Cardinality.East:
                    handleCardinalityEast(drag, modelRef, child, i, sourceIdx);
                    break;
                case Deck.Cardinality.North:
                    handleCardinalityNorth(drag, modelRef, child, i, sourceIdx);
                    break;
                case Deck.Cardinality.South:
                    handleCardinalitySouth(drag, modelRef, child, i, sourceIdx);
                    break;
                }
                return true;
            }
        }

        anchors.fill: parent
        group: root.group

        onEntered: {
            candidate = null;
        }
        onPositionChanged: drag => {
            if (drag.formats.includes("mixxx/player")) {
                return;
            }

            let parentArea = drag.source.parent.mapToItem(grid, 0, 0, drag.source.parent.width, drag.source.parent.height);
            let position = Qt.point(drag.x, drag.y);
            if (position.x < parentArea.x || position.x >= parentArea.x + parentArea.width || position.y < parentArea.y || position.y > parentArea.y + parentArea.height) {
                candidate = null;
                return;
            }

            if (!drag.source.parent.items) {
                console.error(`No items list on the parent of ${drag.source}`);
                candidate = null;
                return;
            }
            let sourceIdx;
            for (let i = 0; i < drag.source.parent.children.length; i++) {
                if (drag.source.parent.children[i] == drag.source) {
                    sourceIdx = i;
                    break;
                }
            }
            if (sourceIdx === null) {
                console.error("Cannot find the source item in the parent's children");
                candidate = null;
                return;
            }
            for (let i = 0; i < drag.source.parent.children.length; i++) {
                if (drag.source.parent.children[i] == drag.source) {
                    continue;
                }
                if (handleChild(i, drag.source.parent.children[i], sourceIdx, position))
                    break;
            }
        }
    }

    component LayoutContainer: GridLayout {
        id: item

        property var beginDrag: null
        property var disposition: GridLayout.TopToBottom
        property alias editOverlay: overlay
        property var move: null
        property bool selected: false

        function complete() {
            if (item.move) {
                item.move.ref.move(item.move.target, item.move.source - item.move.count + 1, item.move.count);
            }
            item.move = null;
        }
        function updateOverlayPosition() {
            let updatedRecursive;
            let updatedItem = item => {
                if (typeof item.selected !== "boolean")
                    return;
                let overlayRect = root.mapFromItem(item, 0, 0, item.width, item.height);
                item.editOverlay.x = overlayRect.x;
                item.editOverlay.y = overlayRect.y;
                item.editOverlay.width = overlayRect.width;
                item.editOverlay.height = overlayRect.height;
                updatedRecursive(item.children);
            };
            updatedRecursive = children => {
                for (let i = 1; i < children.length; i++) {
                    updatedItem(children[i]);
                }
            };
            updatedItem(this);
        }

        Drag.active: mouseArea.drag.active
        Drag.hotSpot: Qt.point(width / 2, height / 2)
        columns: disposition == GridLayout.TopToBottom ? 1 : items.count
        visible: typeof minWidth !== 'number' || root.width <= 0 || minWidth < root.width

        Behavior on x {
            SpringAnimation {
                id: xAnimation

                damping: 0.2
                duration: 500
                spring: 2

                onRunningChanged: {
                    if (running)
                        return;
                    item.complete();
                }
            }
        }
        Behavior on y {
            SpringAnimation {
                id: yAnimation

                damping: 0.2
                duration: 500
                spring: 2

                onRunningChanged: {
                    if (running)
                        return;
                    item.complete();
                }
            }
        }

        onHeightChanged: {
            updateOverlayPosition();
        }
        onWidthChanged: {
            updateOverlayPosition();
        }
        onXChanged: {
            updateOverlayPosition();
        }
        onYChanged: {
            updateOverlayPosition();
        }

        Repeater {
            delegate: deckItemDelegate
            model: parent.visible ? items : []
        }
        Rectangle {
            id: overlay

            color: Qt.alpha(Theme.accent, 0.2)
            parent: root
            visible: root.editMode && selected

            LayoutMouseArea {
                id: mouseArea

                anchors.fill: parent
                drag.target: item
                target: item
            }
        }
    }
    component LayoutItem: Item {
        id: layoutItem

        property var beginDrag: null
        property int blurRadius: 0
        default property var contentChildren
        property alias editLabel: labelItem
        property alias editOverlay: overlayItem
        property alias innerItem: content
        property var move: null

        function complete() {
            if (layoutItem.move) {
                layoutItem.move.ref.move(layoutItem.move.target, layoutItem.move.source - layoutItem.move.count + 1, layoutItem.move.count);
            }
            layoutItem.move = null;
        }

        Drag.active: mouseArea.drag.active
        Drag.hotSpot: Qt.point(width / 2, height / 2)
        visible: typeof minWidth !== 'number' || root.width <= 0 || minWidth < root.width

        Behavior on x {
            SpringAnimation {
                id: xAnimation

                damping: 0.2
                duration: 500
                spring: 2

                onRunningChanged: {
                    if (running)
                        return;
                    layoutItem.complete();
                }
            }
        }
        Behavior on y {
            SpringAnimation {
                id: yAnimation

                damping: 0.2
                duration: 500
                spring: 2

                onRunningChanged: {
                    if (running)
                        return;
                    layoutItem.complete();
                }
            }
        }

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
        GaussianBlur {
            anchors.fill: layoutItem
            deviation: 4
            radius: blurRadius
            samples: 16
            source: content
            visible: root.editMode
        }
        Rectangle {
            id: overlayItem

            anchors.fill: layoutItem
            color: Qt.alpha("black", 0.4)
            visible: root.editMode
            z: 90

            FadeBehavior on visible {
                fadeTarget: overlayItem
            }

            Text {
                id: labelItem

                anchors.centerIn: parent
                font.family: Theme.fontFamily
                visible: !!text.length
            }
            LayoutMouseArea {
                id: mouseArea

                // enabled: overlayItem.visible
                anchors.fill: overlayItem
                drag.target: layoutItem
                target: layoutItem
            }
        }
    }
    component LayoutMouseArea: MouseArea {
        property Item target: parent

        function selectParent() {
            let currentParent = target.parent;
            while (currentParent != root) {
                if (currentParent.selected === false) {
                    let updatedRecursive = children => {
                        for (let i = 1; i < children.length; i++) {
                            if (children[i].selected === true)
                                children[i].selected = false;
                            updatedRecursive(children[i].children);
                        }
                    };
                    updatedRecursive(currentParent.children);
                    currentParent.selected = true;
                    break;
                }
                currentParent = currentParent.parent;
            }
        }

        onPressAndHold: () => {
            selectParent();
            if (!target.beginDrag) {
                target.beginDrag = Qt.point(target.x, target.y);
            }
            target.z = 100;
        }
        onPressed: event => {
            if (event.modifiers & Qt.ControlModifier) {
                selectParent();
            }

            if (!target.beginDrag) {
                target.beginDrag = Qt.point(target.x, target.y);
            }
            target.z = 100;
        }
        onReleased: event => {
            if (target.beginDrag) {
                target.x = target.beginDrag.x;
                target.y = target.beginDrag.y;
                target.beginDrag = null;
            }
            if (!(event.modifiers & Qt.ControlModifier) && target.selected) {
                target.selected = false;
            }

            target.z = 1;
        }
    }
}
