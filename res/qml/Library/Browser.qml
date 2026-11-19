import ".." as Skin
import Mixxx 1.0 as Mixxx
import Qt.labs.qmlmodels
import QtQml
import QtQuick
import QtQml.Models
import QtQuick.Layouts
import QtQuick.Controls 2.15
import Qt5Compat.GraphicalEffects
import "../Theme"

/// Library source tree (All tracks, playlists, crates, ...).
Rectangle {
    id: root

    readonly property var featureSelection: ItemSelectionModel {
        model: root.model
    }
    required property var model
    // The library search sits with the sources now that the full-width library
    // toolbar is gone. The owning pane drives the actual search; this only
    // reports edits so the debounce stays in one place.
    readonly property alias searchInput: searchField
    /// True while the tree owns keyboard focus; Library/Control.qml mirrors
    /// this into [Library],focused_widget.
    readonly property bool treeHasFocus: featureView.activeFocus
    readonly property bool searchHasFocus: searchField.activeFocus
    /// View row that is current. TreeView.rowAtIndex() is Qt 6.5+ and the
    /// project minimum is 6.4, so the delegates report their own row instead.
    property int currentRow: -1
    /// Whether the current source has children, i.e. GoToItem should expand
    /// it instead of jumping to its track list.
    property bool currentHasChildren: false

    signal searchCleared
    signal searchEdited

    function activateDefaultSource() {
        if (!root.model || root.model.tracklist) {
            return;
        }
        // activate() ignores invalid indexes, so no validity check is needed.
        root.model.activate(root.model.index(0, 0));
    }
    function openSourceMenu(index) {
        if (!index || !index.valid || !root.model) {
            return;
        }
        // Keep the model index itself: index.row is the row inside the parent,
        // not the view row, so rebuilding it from row/column would act on a
        // different source (e.g. the first playlist instead of the fourth).
        sourceMenu.sourceIndex = index;
        sourceMenu.nodes = root.model.contextMenu(index);
        if (sourceMenu.nodes.length === 0) {
            return;
        }
        sourceMenu.rebuild();
        sourceMenu.popup();
    }
    function selectRow(index) {
        if (!index) {
            return;
        }
        featureSelection.setCurrentIndex(index, ItemSelectionModel.NoUpdate);
        featureSelection.select(index, ItemSelectionModel.ClearAndSelect | ItemSelectionModel.Rows);
    }
    function showDropMessage(message) {
        dropMessage.messageText = message;
        dropMessageTimer.restart();
    }
    function openSourceSettings() {
        sourceSettings.refresh();
        sourceSettings.open();
    }
    /// Reveals a source a feature asked to select (SidebarModel::selectIndex),
    /// e.g. the playlist that was just renamed. Those edits reset the sidebar
    /// model, which clears both the selection and the expanded state.
    function revealSource(index, scrollTo) {
        if (!index || !index.valid) {
            return;
        }
        // Populate lazy children (folders, crates, ...) first: the view cannot
        // expand a row the model has not created yet.
        for (let ancestor = root.model.parentIndex(index); ancestor && ancestor.valid; ancestor = root.model.parentIndex(ancestor)) {
            root.model.expand(ancestor);
        }
        featureView.expandToIndex(index);
        root.selectRow(index);
        if (!scrollTo) {
            return;
        }
        featureView.forceLayout();
        const row = featureView.rowAtIndex(index);
        if (row >= 0) {
            featureView.positionViewAtRow(row, Qt.AlignVCenter);
        }
    }
    function toggleNode(row) {
        if (featureView.isExpanded(row)) {
            featureView.collapse(row);
            return;
        }
        // Lazy sources (crates, playlists, folders, ...) only create their
        // children when the item is expanded. Do that before expanding the
        // view so the view can take the new rows into account.
        featureView.model.expand(featureView.index(row, 0));
        featureView.expand(row);
    }
    function focusSearch() {
        searchField.forceActiveFocus(Qt.ShortcutFocusReason);
    }
    function focusTree() {
        featureView.forceActiveFocus();
    }
    function indexAt(row) {
        if (row < 0) {
            return null;
        }
        const index = featureView.index(row, 0);
        return index && index.valid ? index : null;
    }
    /// Selects and activates a source by visible row, the way the classic
    /// sidebar activates on every Up/Down.
    function selectRowAt(row) {
        const index = root.indexAt(row);
        if (!index) {
            return;
        }
        root.selectRow(index);
        root.model.activate(index);
        featureView.positionViewAtRow(row, TableView.AlignVCenter);
    }
    function moveSelection(delta) {
        if (!root.model || delta === 0) {
            return;
        }
        const count = featureView.rows;
        if (count <= 0) {
            return;
        }
        const base = root.currentRow >= 0 ? root.currentRow : (delta > 0 ? -1 : count);
        root.selectRowAt(Math.max(0, Math.min(count - 1, base + delta)));
    }
    /// Pages the source selection by one viewport.
    function page(delta) {
        root.moveSelection(delta * Math.max(1, Math.floor(featureView.height / 30)));
    }
    /// Left collapses the current source, right expands it.
    function moveHorizontal(delta) {
        const row = root.currentRow;
        const index = root.indexAt(row);
        if (!index) {
            return;
        }
        if (delta > 0) {
            if (!featureView.isExpanded(row)) {
                root.model.expand(index);
                featureView.expand(row);
            }
        } else if (featureView.isExpanded(row)) {
            featureView.collapse(row);
        }
    }
    function toggleSelected() {
        const row = root.currentRow;
        const index = root.indexAt(row);
        if (!index) {
            return;
        }
        // Activate first: the classic sidebar shows the source's tracks and
        // then toggles its expanded state.
        root.model.activate(index);
        root.toggleNode(row);
    }

    color: Theme.surface1

    Component.onCompleted: root.activateDefaultSource()

    Connections {
        function onModelReset() {
            root.activateDefaultSource();
        }
        // A feature changed a source and wants it shown, e.g. after a rename.
        function onSelectIndex(index, scrollTo) {
            root.revealSource(index, scrollTo);
        }
        // "Sources..." in the source context menu.
        function onSourceSettingsRequested() {
            root.openSourceSettings();
        }

        target: root.model
    }
    Rectangle {
        id: treeFrame

        anchors.fill: parent
        anchors.margins: Theme.space2
        border.color: Theme.border
        border.width: 1
        color: Theme.surface0
        radius: Theme.radiusMd

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Theme.space1
            spacing: 0

            Item {
                id: searchBox

                Layout.fillWidth: true
                Layout.leftMargin: Theme.space1
                Layout.rightMargin: Theme.space1
                Layout.topMargin: Theme.space1
                implicitHeight: 30

                Skin.TextField {
                    id: searchField

                    Accessible.name: qsTr("Search the library")
                    Accessible.role: Accessible.EditableText
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    anchors.right: sourceSettingsButton.left
                    anchors.rightMargin: Theme.space1
                    anchors.top: parent.top
                    font.pixelSize: Theme.fontSizeSm
                    placeholderText: qsTranslate("WSearchLineEdit", "Search...")
                    rightPadding: Theme.space2 + (clearButton.visible ? clearButton.width + Theme.space1 : 0)
                    verticalAlignment: TextInput.AlignVCenter

                    onTextChanged: root.searchEdited()
                }
                Item {
                    id: clearButton

                    Accessible.name: qsTr("Clear search")
                    Accessible.role: Accessible.Button
                    anchors.right: searchField.right
                    anchors.rightMargin: Theme.space2
                    anchors.verticalCenter: parent.verticalCenter
                    height: 14
                    visible: searchField.text.length > 0
                    width: 14

                    Accessible.onPressAction: root.searchCleared()

                    Rectangle {
                        anchors.fill: parent
                        color: clearMouseArea.containsMouse ? Theme.surfaceHover : "transparent"
                        radius: Theme.radiusXs
                    }
                    Rectangle {
                        anchors.centerIn: parent
                        color: clearMouseArea.containsMouse ? Theme.textPrimary : Theme.textMuted
                        height: 1
                        rotation: 45
                        width: 7
                    }
                    Rectangle {
                        anchors.centerIn: parent
                        color: clearMouseArea.containsMouse ? Theme.textPrimary : Theme.textMuted
                        height: 1
                        rotation: -45
                        width: 7
                    }
                    MouseArea {
                        id: clearMouseArea

                        anchors.fill: parent
                        hoverEnabled: true

                        onClicked: root.searchCleared()
                    }
                }
                Rectangle {
                    id: sourceSettingsButton

                    Accessible.name: qsTr("Library sources")
                    Accessible.role: Accessible.Button
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    color: sourceSettingsMouseArea.containsMouse ? Theme.surfaceHover : "transparent"
                    height: 22
                    radius: Theme.radiusXs
                    width: 22

                    Accessible.onPressAction: root.openSourceSettings()

                    Text {
                        anchors.centerIn: parent
                        color: sourceSettingsMouseArea.containsMouse ? Theme.textPrimary : Theme.textMuted
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeSm
                        text: "☰"
                    }
                    MouseArea {
                        id: sourceSettingsMouseArea

                        anchors.fill: parent
                        hoverEnabled: true

                        onClicked: root.openSourceSettings()
                    }
                }
            }
            Rectangle {
                Layout.fillWidth: true
                Layout.leftMargin: Theme.space1
                Layout.rightMargin: Theme.space1
                Layout.topMargin: Theme.space1
                color: Theme.border
                implicitHeight: 1
            }
            ScrollView {
                Layout.fillHeight: true
                Layout.fillWidth: true

                TreeView {
                    id: featureView

                    Layout.fillWidth: true
                    clip: true
                    model: root.model
                    selectionModel: root.featureSelection

                    delegate: FocusScope {
                        required property int column
                        required property bool current
                        required property int depth
                        required property bool expanded
                        required property int hasChildren
                        required property var icon
                        readonly property real indentation: Theme.space3

                        // Rotate indicator when expanded by the user
                        // (requires TreeView to have a selectionModel)
                        property Animation indicatorAnimation: NumberAnimation {
                            duration: Theme.motionFast
                            easing.type: Easing.OutQuart
                            from: expanded ? 0 : 90
                            property: "rotation"
                            target: indicator
                            to: expanded ? 90 : 0
                        }
                        required property bool isTreeNode
                        required property string label
                        readonly property real padding: Theme.space1
                        required property int row
                        // Note: the modelIndex() signature changed with Qt 6.4:
                        // the arguments are (row, column). index() is the
                        // non-deprecated equivalent.
                        readonly property var sourceIndex: treeView.index(row, column)
                        readonly property bool topLevel: depth === 0
                        /// The add action this source offers, rendered as the
                        /// hover "+" button. Empty when it cannot create items.
                        readonly property string addActionText: topLevel && root.model ? root.model.addAction(sourceIndex) : ""
                        /// False for an external library that is not installed
                        /// on this computer: the row stays visible but inert.
                        readonly property bool sourceAvailable: available !== false

                        // Assigned to by TreeView:
                        required property TreeView treeView

                        Accessible.name: label
                        Accessible.role: Accessible.ListItem
                        implicitHeight: topLevel ? 32 : 28
                        implicitWidth: treeView.width

                        TableView.onPooled: indicatorAnimation.complete()
                        TableView.onReused: if (current)
                            indicatorAnimation.start()
                        onCurrentChanged: {
                            if (current) {
                                root.currentRow = row;
                                root.currentHasChildren = hasChildren > 0;
                            }
                        }
                        onExpandedChanged: indicator.rotation = expanded ? 90 : 0

                        Rectangle {
                            id: background

                            anchors.fill: parent
                            color: topLevel ? Theme.surface2 : "transparent"
                        }
                        Rectangle {
                            id: rowHighlight

                            anchors.bottom: parent.bottom
                            anchors.bottomMargin: 1
                            anchors.left: parent.left
                            anchors.leftMargin: Theme.space1
                            anchors.right: parent.right
                            anchors.rightMargin: Theme.space1
                            anchors.top: parent.top
                            anchors.topMargin: 1
                            color: current ? Qt.alpha(Theme.accent, 0.18) : rowDropArea.containsDrag ? Qt.alpha(Theme.accent, 0.25) : rowMouseArea.containsMouse ? Theme.surface3 : "transparent"
                            radius: Theme.radiusSm
                        }
                        Rectangle {
                            id: currentMarker

                            anchors.bottom: parent.bottom
                            anchors.bottomMargin: Theme.space1
                            anchors.left: parent.left
                            anchors.leftMargin: Theme.space1
                            anchors.top: parent.top
                            anchors.topMargin: Theme.space1
                            color: Theme.accent
                            radius: Theme.radiusXs
                            visible: current
                            width: 2
                        }
                        MouseArea {
                            id: rowMouseArea

                            acceptedButtons: Qt.LeftButton | Qt.RightButton
                            anchors.fill: parent
                            hoverEnabled: true

                            onClicked: {
                                // Keep the tree keyboard-navigable: the arrow
                                // keys, Enter, F2 and Delete act on the selected
                                // source row.
                                featureView.forceActiveFocus();
                                root.selectRow(sourceIndex);
                                // Selecting a source activates it, i.e. shows its
                                // tracks. Expanding is a separate action.
                                if (mouse.button === Qt.RightButton) {
                                    root.openSourceMenu(sourceIndex);
                                } else if (sourceAvailable) {
                                    root.model.activate(sourceIndex);
                                }
                            }
                            onDoubleClicked: {
                                // Double click also expands sources that
                                // create their children lazily (e.g. folders
                                // which have no children yet).
                                if (isTreeNode) {
                                    root.toggleNode(row);
                                }
                            }
                        }
                        DropArea {
                            id: rowDropArea

                            anchors.fill: parent

                            // Playlists and crates accept dropped tracks and
                            // folders; the feature decides whether it does.
                            onDropped: drop => {
                                if (!drop.hasUrls)
                                    return;
                                if (!root.model.dropUrls(sourceIndex, drop.urls))
                                    root.showDropMessage(qsTr("Nothing was added. Drop audio files or folders onto a playlist or crate."));
                                drop.acceptProposedAction();
                            }
                        }
                        RowLayout {
                            anchors.left: parent.left
                            anchors.leftMargin: Theme.space2 + indentation * depth
                            anchors.right: parent.right
                            anchors.rightMargin: Theme.space2
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: Theme.space1

                            // Source icons ship in their own colours (orange
                            // tracks, red recordings, ...). Tint them with the
                            // accent so the tree reads as one system.
                            Image {
                                Layout.alignment: Qt.AlignVCenter
                                Layout.preferredHeight: 16
                                Layout.preferredWidth: 16
                                asynchronous: true
                                fillMode: Image.PreserveAspectFit
                                layer.enabled: !!icon
                                opacity: sourceAvailable ? 1.0 : 0.4
                                source: icon ?? ""
                                visible: !!icon

                                layer.effect: ColorOverlay {
                                    color: Theme.accent
                                }
                            }
                            Label {
                                id: indicator

                                Layout.alignment: Qt.AlignVCenter
                                color: Theme.textMuted
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeXs
                                text: "▶"
                                visible: isTreeNode && hasChildren

                                MouseArea {
                                    anchors.fill: parent

                                    onClicked: {
                                        // Like clicking the row in the classic
                                        // sidebar: select it, show its tracks and
                                        // toggle the expanded state.
                                        root.selectRow(sourceIndex);
                                        root.model.activate(sourceIndex);
                                        root.toggleNode(row);
                                    }
                                }
                            }
                            Label {
                                id: labelItem

                                Layout.alignment: Qt.AlignVCenter
                                Layout.fillWidth: true
                                color: topLevel
                                       ? sourceAvailable ? Theme.textPrimary : Theme.textMuted
                                       : Theme.textSecondary
                                elide: Text.ElideRight
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeMd
                                font.weight: topLevel ? Font.DemiBold : Font.Medium
                                text: label
                            }
                            Item {
                                Layout.preferredHeight: 20
                                Layout.preferredWidth: 20
                                visible: rowMouseArea.containsMouse && addActionText !== ""
                            }
                        }
                        Rectangle {
                            id: newItem

                            Accessible.name: qsTr("Create")
                            Accessible.role: Accessible.Button
                            anchors.right: parent.right
                            anchors.rightMargin: Theme.space2
                            anchors.verticalCenter: parent.verticalCenter
                            border.color: newMouseArea.containsMouse ? Theme.accent : Theme.borderStrong
                            border.width: 1
                            color: Theme.surface3
                            height: 20
                            radius: Theme.radiusSm
                            visible: rowMouseArea.containsMouse && addActionText !== ""
                            width: 20

                            Accessible.onPressAction: root.model.invokeContextMenuAction(sourceIndex, addActionText)

                            Text {
                                anchors.centerIn: parent
                                color: newMouseArea.containsMouse ? Theme.accent : Theme.textSecondary
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeMd
                                text: "+"
                            }
                            MouseArea {
                                id: newMouseArea

                                anchors.fill: parent
                                hoverEnabled: true

                                onClicked: root.model.invokeContextMenuAction(sourceIndex, addActionText)
                            }
                        }
                    }

                    Keys.onPressed: event => {
                        const currentIndex = root.featureSelection.currentIndex;
                        // Keyboard context menu for the current source row.
                        if (event.key === Qt.Key_Menu || (event.key === Qt.Key_F10 && (event.modifiers & Qt.ShiftModifier))) {
                            root.openSourceMenu(currentIndex);
                            event.accepted = true;
                            return;
                        }
                        // Same shortcuts as the classic sidebar: F2 renames,
                        // Delete removes the selected playlist or crate.
                        if (event.key === Qt.Key_F2 || event.key === Qt.Key_Delete) {
                            if (currentIndex.valid) {
                                root.model.invokeContextMenuAction(currentIndex, event.key === Qt.Key_F2 ? "rename" : "remove");
                                event.accepted = true;
                            }
                            return;
                        }
                        if (!root.model || !currentIndex.valid) {
                            return;
                        }
                        const currentRow = featureView.rowAtIndex(currentIndex);
                        let targetRow = currentRow;
                        switch (event.key) {
                        case Qt.Key_Up:
                            targetRow = currentRow - 1;
                            break;
                        case Qt.Key_Down:
                            targetRow = currentRow + 1;
                            break;
                        case Qt.Key_Home:
                            targetRow = 0;
                            break;
                        case Qt.Key_End:
                            targetRow = featureView.rows - 1;
                            break;
                        case Qt.Key_PageUp:
                            targetRow = currentRow - 10;
                            break;
                        case Qt.Key_PageDown:
                            targetRow = currentRow + 10;
                            break;
                        case Qt.Key_Return:
                        case Qt.Key_Enter:
                            root.toggleNode(currentRow);
                            event.accepted = true;
                            return;
                        default:
                            return;
                        }
                        targetRow = Math.min(Math.max(targetRow, 0), featureView.rows - 1);
                        // Moving the selection shows the source, like the
                        // classic sidebar's pressed -> clicked timer.
                        const targetIndex = featureView.index(targetRow, 0);
                        root.selectRow(targetIndex);
                        root.model.activate(targetIndex);
                        event.accepted = true;
                    }
                }
            }
        }
    }
    Rectangle {
        id: dropMessage

        property string messageText: ""

        Accessible.name: messageText
        Accessible.role: Accessible.StaticText
        anchors.bottom: treeFrame.bottom
        anchors.bottomMargin: Theme.space2
        anchors.horizontalCenter: parent.horizontalCenter
        border.color: Theme.borderStrong
        border.width: 1
        color: Theme.surface3
        height: 26
        radius: Theme.radiusSm
        visible: messageText.length > 0 && dropMessageTimer.running
        width: Math.min(treeFrame.width - 2 * Theme.space2, dropMessageLabel.implicitWidth + 2 * Theme.space3)
        z: 2

        Text {
            id: dropMessageLabel

            anchors.centerIn: parent
            color: Theme.textPrimary
            elide: Text.ElideRight
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeSm
            text: dropMessage.messageText
            width: parent.width - 2 * Theme.space2
        }
    }
    Timer {
        id: dropMessageTimer

        interval: 4000
    }
    Menu {
        id: sourceMenu

        property var nodes: []
        property var sourceIndex: null

        function rebuild() {
            while (count > 0) {
                removeItem(itemAt(0));
            }
            for (let i = 0; i < nodes.length; ++i) {
                const node = nodes[i];
                if (node.kind === "separator") {
                    addItem(separatorComponent.createObject(sourceMenu));
                    continue;
                }
                const item = actionComponent.createObject(sourceMenu);
                item.nodeAction = node.action;
                item.text = node.text;
                item.enabled = node.enabled;
                addItem(item);
            }
        }

        palette.buttonText: Theme.textPrimary
        palette.highlight: Theme.accent
        palette.highlightedText: Theme.textOnAccent
        palette.mid: Theme.textMuted
        palette.text: Theme.textPrimary
        palette.window: Theme.surface2
        palette.windowText: Theme.textPrimary

        background: Rectangle {
            border.color: Theme.border
            border.width: 1
            color: Theme.surface2
            implicitWidth: 260
            radius: Theme.radiusMd
        }

        Component {
            id: separatorComponent

            MenuSeparator {
            }
        }
        Component {
            id: actionComponent

            MenuItem {
                id: actionItem

                property string nodeAction: ""

                onTriggered: root.model.invokeContextMenuAction(sourceMenu.sourceIndex, actionItem.nodeAction)
            }
        }
    }
    SourceSettings {
        id: sourceSettings

        sidebar: root.model
    }
}
