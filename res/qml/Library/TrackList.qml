import ".." as Skin
import "." as LibraryComponent
import Mixxx 1.0 as Mixxx
import Qt.labs.qmlmodels
import QtQml
import QtQuick
import QtQml.Models
import QtQuick.Layouts
import QtQuick.Controls 2.15
import "../Theme"

/// Track list with a flat header bar, zebra rows and an accent selection.
Rectangle {
    id: root

    /// Row the context menu was last opened for. Used to restore a usable
    /// selection after an action changed the underlying model.
    property int contextRow: -1
    required property var model
    /// Sidebar proxy. Column order and visibility are shared by every source,
    /// so the header edits the sidebar, not just this view's model.
    property var sidebar
    /// Width a filled column keeps before the table scrolls horizontally.
    readonly property int minimumColumnWidth: 64
    /// Gap under the pointer while a header is dragged; -1 when idle.
    property int columnDropSlot: -1
    property real columnDropX: 0
    /// Rows are uniform, so keyboard paging and scrolling can be computed
    /// without a row height provider.
    readonly property int rowHeight: 28
    /// Capabilities are per model, not per cell. Resolve them once here
    /// instead of invoking the model from every delegate.
    readonly property var capabilities: model ? model.getCapabilities() : Mixxx.LibraryTrackListModel.Capability.None
    /// Set while a pointer gesture started with a selection modifier. The
    /// column delegates load Track.qml, whose own tap handler calls
    /// selectionModel.selectRow() (a plain replacement) when the button is
    /// released. Without this guard that call would wipe the multi-selection
    /// the modified click builds. It is recomputed on every press, so a stale
    /// value can never leak into the next gesture.
    property bool suppressPlainSelect: false
    /// Column that renders the title text. The stem badge is drawn in its
    /// left gutter, so it appears exactly once per row instead of on every
    /// cell. Looked up by SQL column instead of by index so a reordered or
    /// extended column set in `SourceTree` keeps working; -1 (no title
    /// column, e.g. while no source is loaded) simply hides the badge.
    readonly property int titleColumnIndex: {
        const columns = root.model ? root.model.columns : null;
        if (!columns) {
            return -1;
        }
        for (let i = 0; i < columns.length; ++i) {
            if (columns[i] && columns[i].columnIdx === Mixxx.TrackListColumn.SQLColumns.Title) {
                return i;
            }
        }
        return -1;
    }

    /// State a mutating action is about to disturb: the scroll anchor and the
    /// selection. Both are lost when the model reloads after the edit.
    function captureViewState() {
        return {
            "topRow": Math.max(0, view.topRow),
            "rows": view.selectionModel.selectedRows()
        };
    }
    function handleRowClick(row, modifiers) {
        // Without this Up/Down stop working as soon as the mouse was used.
        view.forceActiveFocus();
        if (modifiers & Qt.ShiftModifier) {
            view.selectionModel.extendTo(row);
        } else if (modifiers & (Qt.ControlModifier | Qt.MetaModifier)) {
            view.selectionModel.toggleRow(row);
        } else {
            view.selectionModel.selectSingleton(row);
        }
    }
    function hasSelectionModifier(modifiers) {
        return (modifiers & (Qt.ShiftModifier | Qt.ControlModifier | Qt.MetaModifier)) !== 0;
    }
    /// Restores `state` after the model changed. `rows` reselects that range;
    /// an empty list keeps the previous selection where it is still valid.
    function restoreViewState(state, rows) {
        view.forceLayout();
        const selection = view.selectionModel;
        const rowCount = selection.modelRowCount();
        if (rowCount === 0)
            return;
        const wanted = rows && rows.length > 0 ? rows : state.rows;
        const valid = [];
        for (let i = 0; i < wanted.length; ++i) {
            const row = wanted[i];
            if (row >= 0 && row < rowCount && valid.indexOf(row) < 0)
                valid.push(row);
        }
        if (valid.length > 0) {
            selection.clear();
            const flags = ItemSelectionModel.Rows | ItemSelectionModel.Select;
            for (let i = 0; i < valid.length; ++i)
                selection.select(selection.model.index(valid[i], 0), flags);
            selection.setCurrentIndex(selection.model.index(valid[0], 0), ItemSelectionModel.NoUpdate);
            selection.anchorRow = valid[0];
        }
        view.positionViewAtRow(Math.min(Math.max(0, state.topRow), rowCount - 1), TableView.AlignTop, 0);
    }
    function selectRow(row) {
        if (root.suppressPlainSelect)
            return;
        view.selectionModel.selectSingleton(row);
    }
    function showDropMessage(message) {
        dropMessage.text = message;
        dropMessageTimer.restart();
    }
    function showTrackMenu(row, position) {
        view.forceActiveFocus();
        root.contextRow = row;
        const selection = view.selectionModel;
        if (!selection.isRowSelected(row)) {
            // Right-clicking outside the selection acts on that row only.
            selection.selectSingleton(row);
        } else {
            // Keep the multi-selection and move the current row to the click.
            selection.setCurrentRow(row);
        }
        if (position) {
            trackMenu.x = position.x;
            trackMenu.y = position.y;
        }
        trackMenu.popupFor(root.model, selection.selectedRows());
    }
    /// Returns the gap under `pointerX` (root coordinates) and the x position
    /// where the drop marker belongs. Hidden columns have no width, so only the
    /// visible cells decide the slot.
    function columnGapBefore(pointerX) {
        const columns = root.model ? root.model.columns : null;
        if (!columns)
            return {"slot": 0, "x": 0};
        // Measure in view content coordinates so a horizontally scrolled table
        // still drops at the header cell under the pointer.
        const viewLeft = view.mapToItem(root, 0, 0).x;
        const contentX = pointerX - viewLeft + view.contentX;
        let x = 0;
        for (let i = 0; i < columns.length; ++i) {
            const width = view.columnWidthProvider(i);
            if (width <= 0)
                continue;
            if (contentX < x + width / 2)
                return {"slot": i, "x": viewLeft + x - view.contentX};
            x += width;
        }
        return {"slot": columns.length, "x": viewLeft + x - view.contentX};
    }
    function updateColumnDrop(pointerX) {
        const gap = root.columnGapBefore(pointerX);
        root.columnDropSlot = gap.slot;
        root.columnDropX = gap.x;
    }
    function moveColumnToSlot(from, slot) {
        root.columnDropSlot = -1;
        if (!root.sidebar || !root.model)
            return;
        // The slot is the gap before a column; after removing the dragged
        // column every later gap moves one to the left.
        let to = slot > from ? slot - 1 : slot;
        to = Math.max(0, Math.min(root.model.columns.length - 1, to));
        if (to === from)
            return;
        // The sort indicator cannot follow the moved column.
        horizontalHeader.sortingColumn = -1;
        root.sidebar.moveColumn(from, to);
        root.afterColumnLayoutChange();
    }
    function afterColumnLayoutChange() {
        view.updateColumnSize();
        view.forceLayout();
    }
    function setColumnHidden(column, hidden) {
        if (!root.sidebar)
            return;
        root.sidebar.setColumnHidden(column, hidden);
        root.afterColumnLayoutChange();
    }
    function openColumnMenu() {
        columnMenu.entries = root.columnMenuEntries();
        // popup() opens at the cursor like the source-tree menu; open() would
        // use a parent-relative position.
        columnMenu.popup();
    }
    function columnMenuEntries() {
        const entries = [];
        if (!root.model)
            return entries;
        const columns = root.model.columns;
        for (let i = 0; i < columns.length; ++i) {
            // The cover thumbnail has no label and is not a selectable column.
            if (columns[i].label.length === 0)
                continue;
            entries.push({
                "column": i,
                "hidden": columns[i].hidden,
                "label": columns[i].label
            });
        }
        return entries;
    }

    color: Theme.surface0

    // Keyboard focus starts on the track list, so the arrows and Delete act on
    // tracks rather than on the source tree. Clicking a source row moves focus
    // there for as long as the user navigates sources.
    Component.onCompleted: view.forceActiveFocus()

    LibraryComponent.Control {
        id: libraryControl

        onFocusWidgetChanged: {
            switch (focusWidget) {
            case Skin.FocusedWidgetControl.WidgetKind.LibraryView:
                view.forceActiveFocus();
                break;
            }
        }
        onLoadSelectedTrack: (group, play) => {
            view.loadSelectedTrack(group, play);
        }
        onLoadSelectedTrackIntoNextAvailableDeck: play => {
            view.loadSelectedTrackIntoNextAvailableDeck(play);
        }
        onMoveVertical: offset => {
            view.selectionModel.moveSelectionVertical(offset);
        }
    }
    Rectangle {
        id: headerBackground

        anchors.left: horizontalHeader.left
        anchors.right: horizontalHeader.right
        anchors.top: horizontalHeader.top
        color: Theme.surface2
        height: horizontalHeader.height

        Rectangle {
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            color: Theme.borderStrong
            height: 1
        }
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            color: Theme.topHighlight
            height: 1
        }
    }
    HorizontalHeaderView {
        id: horizontalHeader
        property int sortingColumn: -1
        property var sortingOrder: Qt.Descending

        anchors.left: parent.left
        anchors.margins: Theme.space1
        anchors.right: parent.right
        anchors.top: parent.top
        syncView: view

        delegate: Item {
            id: column

            required property string display
            required property int index

            /// Set once the pointer moved far enough to be a column drag.
            property bool dragMoved: false
            /// The click that follows a drop must not also sort the column.
            property bool suppressClick: false
            property int pressX: 0
            property int pressedButton: Qt.NoButton

            Accessible.name: qsTr("Sort by %1").arg(display)
            Accessible.role: Accessible.Button
            implicitHeight: 32
            implicitWidth: columnName.contentWidth + Theme.space2
            opacity: column.dragMoved ? 0.45 : 1

            MouseArea {
                id: columnMouseHandler

                acceptedButtons: Qt.LeftButton
                anchors.fill: parent
                hoverEnabled: true

                onClicked: mouse => {
                    if (column.suppressClick) {
                        column.suppressClick = false;
                        return;
                    }
                    // The Auto DJ queue does not advertise sorting: the
                    // processor always plays row 0, so sorting would change the
                    // playback order. Sorting a regular playlist still works.
                    if (!view.model || !view.model.hasCapabilities(Mixxx.LibraryTrackListModel.Capability.Sorting))
                        return;
                    if (horizontalHeader.sortingColumn == index) {
                        horizontalHeader.sortingOrder = horizontalHeader.sortingOrder == Qt.DescendingOrder ? Qt.AscendingOrder : Qt.DescendingOrder;
                    } else {
                        horizontalHeader.sortingColumn = index;
                        horizontalHeader.sortingOrder = Qt.AscendingOrder;
                    }
                    view.model.sort(horizontalHeader.sortingColumn, horizontalHeader.sortingOrder);
                }
                onPositionChanged: mouse => {
                    if (!pressed || column.pressedButton !== Qt.LeftButton)
                        return;
                    if (!column.dragMoved && Math.abs(mouse.x - column.pressX) < 6)
                        return;
                    column.dragMoved = true;
                    const point = column.mapToItem(root, mouse.x, 0);
                    root.updateColumnDrop(point.x);
                }
                onPressed: mouse => {
                    column.dragMoved = false;
                    column.suppressClick = false;
                    column.pressedButton = mouse.button;
                    column.pressX = mouse.x;
                }
                onReleased: mouse => {
                    if (!column.dragMoved || column.pressedButton !== Qt.LeftButton)
                        return;
                    const from = index;
                    const slot = root.columnDropSlot;
                    column.dragMoved = false;
                    column.suppressClick = true;
                    // The reorder rebuilds the header delegates, so move out of
                    // the finished gesture before the view is laid out again.
                    Qt.callLater(() => root.moveColumnToSlot(from, slot));
                }
                onCanceled: {
                    column.dragMoved = false;
                    root.columnDropSlot = -1;
                }
            }
            Text {
                id: columnName

                anchors.fill: parent
                anchors.leftMargin: Theme.space3
                anchors.rightMargin: Theme.space4
                color: columnMouseHandler.containsMouse ? Theme.textPrimary : Theme.textSecondary
                elide: Text.ElideRight
                font.capitalization: Font.AllUppercase
                font.family: Theme.fontFamily
                font.letterSpacing: Theme.letterSpacingCaps
                font.pixelSize: Theme.fontSizeXs
                font.weight: Theme.fontWeightDemiBold
                horizontalAlignment: Text.AlignLeft
                text: display
                verticalAlignment: Text.AlignVCenter
            }
            Text {
                id: sortIndicator

                anchors.right: parent.right
                anchors.rightMargin: Theme.space2
                anchors.verticalCenter: parent.verticalCenter
                color: Theme.accent
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeXs
                text: horizontalHeader.sortingOrder == Qt.AscendingOrder ? "▲" : "▼"
                visible: horizontalHeader.sortingColumn == index
            }
            Rectangle {
                id: columnResizer

                anchors.bottom: parent.bottom
                anchors.right: parent.right
                anchors.top: parent.top
                color: columnResizeHandler.containsMouse || columnResizeHandler.drag.active ? Theme.accent : Theme.border
                width: 1

                MouseArea {
                    id: columnResizeHandler

                    property int sizeOffset: 0

                    anchors.fill: parent
                    cursorShape: Qt.SizeHorCursor
                    hoverEnabled: true
                    preventStealing: true

                    onMouseXChanged: {
                        if (drag.active) {
                            column.width += mouseX;
                            sizeOffset += mouseX;
                        }
                    }

                    drag {
                        axis: Drag.XAxis
                        target: parent
                        threshold: 2

                        onActiveChanged: {
                            if (!drag.active && columnResizeHandler.sizeOffset !== 0) {
                                view.model.columns[index].preferredWidth = column.width;
                                columnResizeHandler.sizeOffset = 0;
                                view.updateColumnSize();
                                view.forceLayout();
                            }
                        }
                    }
                }
            }
        }
    }
    // A right-click anywhere on the header opens the column menu. Accepting
    // only the right button leaves sort, drag and resize clicks to the header
    // delegates underneath.
    MouseArea {
        anchors.fill: horizontalHeader
        acceptedButtons: Qt.RightButton
        z: 5

        onClicked: root.openColumnMenu()
    }
    TableView {
        id: view

        property int dynamicColumnCount: 0
        readonly property int pageRowCount: Math.max(1, Math.floor(height / root.rowHeight))
        property int usedWidth: 0

        function ensureRowVisible(row) {
            if (row < 0)
                return;
            const rowTop = row * root.rowHeight;
            const maxY = Math.max(0, contentHeight - height);
            let y = contentY;
            if (rowTop < y) {
                y = rowTop;
            } else if (rowTop + root.rowHeight > y + height) {
                y = rowTop + root.rowHeight - height;
            }
            contentY = Math.max(0, Math.min(y, maxY));
        }
        function loadSelectedTrack(group, play) {
            const url = this.selectionModel.currentTrackUrl();
            if (!url)
                return;

            Mixxx.PlayerManager.getPlayer(group).loadTrackFromLocationUrl(url, play);
        }
        function loadSelectedTrackIntoNextAvailableDeck(play) {
            const url = this.selectionModel.currentTrackUrl();
            if (!url)
                return;

            Mixxx.PlayerManager.loadLocationUrlIntoNextAvailableDeck(url, play);
        }
        function updateColumnSize() {
            const oldUsedWidth = usedWidth;
            const oldDynamicColumnCount = dynamicColumnCount;
            usedWidth = 0;
            dynamicColumnCount = 0;
            if (model == null) {
                return;
            }
            for (let c = 0; c < model.columns.length; c++) {
                if (model.columns[c].hidden || model.columns[c].autoHideWidth > view.width) {
                    continue;
                } else if (model.columns[c].preferredWidth > 0) {
                    usedWidth += model.columns[c].preferredWidth;
                } else {
                    dynamicColumnCount += model.columns[c].fillSpan || 1;
                }
            }
            return oldDynamicColumnCount != dynamicColumnCount || oldUsedWidth != usedWidth;
        }

        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.margins: Theme.space1
        anchors.right: parent.right
        anchors.top: horizontalHeader.bottom
        clip: true
        columnWidthProvider: function (column) {
            const columnDef = view.model.columns[column];
            if (columnDef.hidden) {
                return 0;
            }
            if (columnDef.autoHideWidth > 0 && columnDef.autoHideWidth > view.width) {
                return 0;
            }
            if (columnDef.preferredWidth >= 0) {
                return columnDef.preferredWidth;
            }
            const span = columnDef.fillSpan || 1;
            return Math.max(root.minimumColumnWidth,
                    span * (view.width - view.usedWidth) / view.dynamicColumnCount);
        }
        keyNavigationEnabled: false
        model: root.model
        pointerNavigationEnabled: false
        reuseItems: true

        ScrollBar.horizontal: ScrollBar {
            id: horizontalScrollBar

            policy: ScrollBar.AsNeeded
            height: 10

            background: Rectangle {
                color: "transparent"
            }
            contentItem: Rectangle {
                color: horizontalScrollBar.pressed ? Theme.accent : Theme.borderStrong
                implicitHeight: 4
                radius: Theme.radiusXs
            }
        }
        ScrollBar.vertical: ScrollBar {
            id: verticalScrollBar

            policy: ScrollBar.AlwaysOn
            width: 10

            background: Rectangle {
                color: "transparent"
            }
            contentItem: Rectangle {
                color: verticalScrollBar.pressed ? Theme.accent : Theme.borderStrong
                implicitWidth: 4
                radius: Theme.radiusXs
            }
        }
        delegate: Item {
            id: item

            required property int column
            required property var display
            required property bool has_stems

            /// Keyboard modifiers of the press that started the current
            /// gesture. Captured on press because the tapped signal only
            /// carries the event point and the button.
            property int pressModifiers: Qt.NoModifier
            required property int row
            required property bool selected
            required property var track

            implicitHeight: root.rowHeight

            Loader {
                id: loader

                property var capabilities: root.capabilities
                property var display: item.display
                /// `has_stems` from the track list model, mirrored for the
                /// column delegates (see `SourceTree.qml`).
                property bool has_stems: item.has_stems
                property int row: item.row
                property bool selected: item.selected
                /// True for the single cell that owns the per-row stem badge.
                property bool stemBadgeVisible: item.has_stems && item.column === root.titleColumnIndex
                property var tableView: view
                property var libraryList: root
                property var track: item.track

                anchors.fill: parent
                sourceComponent: delegate

                onLoaded:
                // Workaround needed for WaveformOverview column to load the data
                //     if (track)
                //         Mixxx.Library.analyze(track)
                {}
            }
            // One decorative badge per row, in the left gutter of the title
            // column. No pointer handlers, so row clicks/drags are unaffected.
            // Instantiate it only for rows that actually have stems; a Repeater
            // and three rectangles in every cell is a lot of unused items.
            Loader {
                id: stemBadgeLoader

                active: loader.stemBadgeVisible
                anchors.verticalCenter: parent.verticalCenter
                height: 12
                visible: active
                width: 12
                x: Theme.space1

                sourceComponent: Component {
                    Item {
                        Accessible.ignored: true

                        Column {
                            anchors.centerIn: parent
                            spacing: 1

                            Repeater {
                                model: 3

                                Rectangle {
                                    color: Theme.accent
                                    height: 2
                                    radius: 1
                                    width: 10
                                }
                            }
                        }
                    }
                }
            }
            // Workaround needed for WaveformOverview column to load the data
            // TableView.onReused: {
            //     if (track)
            //         Mixxx.Library.analyze(track)
            // }
            TapHandler {
                id: rowTapHandler

                acceptedButtons: Qt.LeftButton | Qt.RightButton

                onLongPressed: root.showTrackMenu(item.row, item.mapToItem(root, rowTapHandler.point.position))
                onPressedChanged: {
                    if (!rowTapHandler.pressed)
                        return;
                    item.pressModifiers = rowTapHandler.point.modifiers;
                    root.suppressPlainSelect = root.hasSelectionModifier(item.pressModifiers);
                }
                onTapped: (eventPoint, button) => {
                    if (button === Qt.RightButton) {
                        root.showTrackMenu(item.row, item.mapToItem(root, rowTapHandler.point.position));
                    } else if (button === Qt.LeftButton) {
                        root.handleRowClick(item.row, item.pressModifiers);
                    }
                }
            }
        }
        selectionModel: ItemSelectionModel {
            id: selectionModel

            /// Row a Shift+click range grows from. Maintained here instead of
            /// being derived from the current index, so that Ctrl+click and
            /// Shift+click behave like the classic track table.
            property int anchorRow: -1

            function clearSelectionAndCurrent() {
                this.clear();
                this.anchorRow = -1;
            }
            function currentRow() {
                const index = this.currentIndex;
                return index ? index.row : -1;
            }
            function currentTrackUrl() {
                const row = this.effectiveCurrentRow();
                if (row < 0)
                    return null;
                return this.model.getUrl(row);
            }
            function effectiveCurrentRow() {
                const row = this.currentRow();
                if (row >= 0)
                    return row;
                const rows = this.selectedRows();
                return rows.length > 0 ? rows[0] : -1;
            }
            function extendTo(row) {
                const newRow = this.normalizeRow(row);
                if (newRow < 0)
                    return;
                if (this.anchorRow < 0)
                    this.anchorRow = Math.max(0, this.effectiveCurrentRow());
                const first = Math.min(this.anchorRow, newRow);
                const last = Math.max(this.anchorRow, newRow);
                this.clear();
                const flags = ItemSelectionModel.Rows | ItemSelectionModel.Select;
                for (let r = first; r <= last; ++r)
                    this.select(this.model.index(r, 0), flags);
                this.setCurrentIndex(this.model.index(newRow, 0), ItemSelectionModel.NoUpdate);
            }
            function modelRowCount() {
                return this.model ? this.model.rowCount() : 0;
            }
            function moveSelectionTo(row, extend) {
                this.moveToRow(row, extend);
            }
            /// Moves the current row by `offset` rows and selects it, unless
            /// `extend` is set, in which case the selection is grown from the
            /// anchor like Shift+click.
            function moveSelectionVertical(offset, extend) {
                if (offset === 0)
                    return;
                const base = Math.max(0, this.effectiveCurrentRow());
                this.moveToRow(base + offset, extend);
            }
            function moveToRow(row, extend) {
                const rowCount = this.modelRowCount();
                if (rowCount === 0)
                    return;
                const target = Math.max(0, Math.min(rowCount - 1, row));
                if (extend) {
                    this.extendTo(target);
                } else {
                    this.selectSingleton(target);
                }
                view.ensureRowVisible(target);
            }
            function normalizeRow(row) {
                const rowCount = this.modelRowCount();
                if (rowCount === 0)
                    return -1;
                return Mixxx.MathUtils.positiveModulo(row, rowCount);
            }
            function selectAllRows() {
                const rowCount = this.modelRowCount();
                if (rowCount === 0)
                    return;
                const current = Math.max(0, this.effectiveCurrentRow());
                this.clear();
                const flags = ItemSelectionModel.Rows | ItemSelectionModel.Select;
                for (let r = 0; r < rowCount; ++r)
                    this.select(this.model.index(r, 0), flags);
                this.setCurrentIndex(this.model.index(current, 0), ItemSelectionModel.NoUpdate);
                this.anchorRow = current;
            }
            function selectRow(row) {
                // Plain replacement selection. The column delegates call this
                // from their own tap handler; a modified click has already
                // applied its selection, so it must not replace it here.
                if (root.suppressPlainSelect)
                    return;
                this.selectSingleton(row);
            }
            function selectSingleton(row) {
                const newRow = this.normalizeRow(row);
                if (newRow < 0) {
                    this.clear();
                    this.anchorRow = -1;
                    return;
                }
                this.anchorRow = newRow;
                this.select(this.model.index(newRow, 0), ItemSelectionModel.Rows | ItemSelectionModel.Select | ItemSelectionModel.Clear | ItemSelectionModel.Current);
            }
            /// Selected rows without the per-column duplicates that
            /// selectedIndexes returns for ItemSelectionModel.Rows. Uses a
            /// lookup object because a Select All can span every row.
            function selectedRows() {
                const seen = {};
                const indexes = this.selectedIndexes;
                for (let i = 0; i < indexes.length; ++i)
                    seen[indexes[i].row] = true;
                return Object.keys(seen).map(row => Number(row)).sort((a, b) => a - b);
            }
            function selectedTrackUrls() {
                const rows = this.selectedRows();
                const urls = [];
                for (let i = 0; i < rows.length; ++i)
                    urls.push(this.model.getUrl(rows[i]));
                return urls;
            }
            function setCurrentRow(row) {
                const newRow = this.normalizeRow(row);
                if (newRow < 0)
                    return;
                this.setCurrentIndex(this.model.index(newRow, 0), ItemSelectionModel.NoUpdate);
            }
            function toggleRow(row) {
                const newRow = this.normalizeRow(row);
                if (newRow < 0)
                    return;
                const index = this.model.index(newRow, 0);
                if (this.isSelected(index)) {
                    this.select(index, ItemSelectionModel.Rows | ItemSelectionModel.Deselect | ItemSelectionModel.Current);
                } else {
                    this.select(index, ItemSelectionModel.Rows | ItemSelectionModel.Select | ItemSelectionModel.Current);
                }
                this.anchorRow = newRow;
            }

            model: view.model
        }

        Component.onCompleted: this.updateColumnSize()
        Keys.onPressed: event => {
            const selection = view.selectionModel;
            const extend = (event.modifiers & Qt.ShiftModifier) !== 0;
            switch (event.key) {
            case Qt.Key_Up:
                selection.moveSelectionVertical(-1, extend);
                break;
            case Qt.Key_Down:
                selection.moveSelectionVertical(1, extend);
                break;
            case Qt.Key_PageUp:
                selection.moveSelectionVertical(-view.pageRowCount, extend);
                break;
            case Qt.Key_PageDown:
                selection.moveSelectionVertical(view.pageRowCount, extend);
                break;
            case Qt.Key_Home:
                selection.moveSelectionTo(0, extend);
                break;
            case Qt.Key_End:
                selection.moveSelectionTo(selection.modelRowCount() - 1, extend);
                break;
            case Qt.Key_A:
                if ((event.modifiers & (Qt.ControlModifier | Qt.MetaModifier)) === 0)
                    return;
                selection.selectAllRows();
                break;
            case Qt.Key_Escape:
                selection.clearSelectionAndCurrent();
                break;
            case Qt.Key_Return:
            case Qt.Key_Enter:
                if ((event.modifiers & (Qt.ControlModifier | Qt.MetaModifier)) !== 0) {
                    Mixxx.LibraryTrackMenu.showProperties(root.model, selection.selectedRows());
                } else {
                    view.loadSelectedTrackIntoNextAvailableDeck(false);
                }
                break;
            case Qt.Key_Delete:
            case Qt.Key_Backspace:
                {
                    const rows = selection.selectedRows();
                    if (rows.length === 0)
                        break;
                    const state = root.captureViewState();
                    // removeSelectedRows() confirms and hides/removes according to
                    // the capabilities of the active source.
                    Mixxx.LibraryTrackMenu.removeSelectedRows(root.model, rows);
                    // Select the row that took the place of the first removed one.
                    root.restoreViewState(state, [Math.min(rows[0], selection.modelRowCount() - 1)]);
                    break;
                }
            default:
                return;
            }
            event.accepted = true;
        }
        onModelChanged: this.updateColumnSize()
        onWidthChanged: {
            if (view.updateColumnSize()) {
                // forceLayout is costly - only invoke if there was a change in the column layouts
                view.forceLayout();
            }
        }

        DropArea {
            id: trackDropArea

            anchors.fill: parent

            // Rows dragged out of this table are reordered; anything else is
            // imported into the source shown here.
            onDropped: drop => {
                const rowCount = view.selectionModel.modelRowCount();
                const destination = Math.max(0, Math.min(rowCount, Math.floor((drop.y + view.contentY) / root.rowHeight)));
                const state = root.captureViewState();
                if (drop.source && drop.source.trackList === view) {
                    if (!root.model || !root.model.hasCapabilities(Mixxx.LibraryTrackListModel.Capability.Reorder))
                        return;
                    const rows = view.selectionModel.selectedRows();
                    if (rows.length === 0)
                        return;
                    Mixxx.LibraryTrackMenu.moveSelectedRows(root.model, rows, destination);
                    // The block starts at the drop row, or is shifted up by its
                    // own length when it was moved down.
                    let start = destination;
                    if (destination > rows[rows.length - 1])
                        start = destination - rows.length;
                    const moved = [];
                    for (let i = 0; i < rows.length; ++i)
                        moved.push(start + i);
                    root.restoreViewState(state, moved);
                    drop.acceptProposedAction();
                    return;
                }
                if (!drop.hasUrls)
                    return;
                const count = Mixxx.LibraryTrackMenu.importDroppedUrls(root.model, drop.urls, destination);
                if (count === 0) {
                    root.showDropMessage(qsTr("Nothing was added. Drop audio files or folders onto an unlocked view."));
                } else if (root.model.hasCapabilities(Mixxx.LibraryTrackListModel.Capability.Reorder)) {
                    // Playlists keep insertion order, so the new rows are known.
                    const added = [];
                    for (let i = 0; i < count; ++i)
                        added.push(Math.min(destination, rowCount) + i);
                    root.restoreViewState(state, added);
                } else {
                    // A sorted source places the tracks wherever they belong.
                    root.restoreViewState(state, []);
                }
                drop.acceptProposedAction();
            }
            onEntered: drag => {
                // Accept URL drops and internal row drags only.
                drag.accepted = drag.hasUrls || (drag.source && drag.source.trackList === view);
            }
        }
    }
    Connections {
        function onRestoreSelectionRequested() {
            // An action such as "Hide from Library" reloaded the model, which
            // drops the selection. Restore it around the row it was invoked on.
            view.forceActiveFocus();
            const selection = view.selectionModel;
            const rowCount = selection.modelRowCount();
            if (rowCount === 0)
                return;
            const row = root.contextRow >= 0 ? Math.min(root.contextRow, rowCount - 1) : 0;
            selection.selectSingleton(row);
            view.ensureRowVisible(row);
        }

        target: Mixxx.LibraryTrackMenu
    }
    LibraryComponent.TrackMenu {
        id: trackMenu
    }
    // One drag preview for the whole list. It used to be instantiated in every
    // cell (a hidden rectangle, two labels and a layout), which added several
    // thousand items to a full viewport for an overlay that only exists while
    // the user is dragging a row.
    Item {
        id: dragPreview

        property string artistText
        property string titleText
        height: 85
        // Grabbed behind the table. Rendering is only needed for the grab, so
        // the preview never participates in the visible layout.
        visible: false
        width: 190
        x: 0
        y: 0
        z: -1

        Rectangle {
            anchors.fill: parent
            anchors.margins: Theme.space1
            border.color: Theme.border
            border.width: 1
            color: Theme.surface2
            radius: Theme.radiusMd

            Column {
                anchors.fill: parent
                anchors.margins: Theme.space2
                spacing: 0

                Text {
                    color: Theme.textPrimary
                    elide: Text.ElideRight
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSm
                    font.weight: Theme.fontWeightDemiBold
                    text: dragPreview.titleText.length > 0 ? dragPreview.titleText : qsTr("Unknown title")
                    width: parent.width
                }
                Text {
                    color: Theme.textSecondary
                    elide: Text.ElideRight
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeXs
                    text: dragPreview.artistText.length > 0 ? dragPreview.artistText : qsTr("Unknown artist")
                    width: parent.width
                }
            }
        }
    }
    function captureDragPreview(track, callback) {
        dragPreview.titleText = track ? track.title : "";
        dragPreview.artistText = track ? track.artist : "";
        // Render the off-screen preview for one frame so grabToImage sees a
        // fully laid-out item, then hide it again.
        dragPreview.visible = true;
        Qt.callLater(function() {
            dragPreview.grabToImage(function(result) {
                dragPreview.visible = false;
                callback(result.url);
            }, Qt.size(dragPreview.width, dragPreview.height));
        });
    }
    // Drop marker while a column header is dragged.
    Rectangle {
        id: columnDropIndicator

        color: Theme.accent
        height: horizontalHeader.height
        visible: root.columnDropSlot >= 0
        width: 2
        x: root.columnDropX - width / 2
        y: horizontalHeader.y
        z: 20
    }
    // Right-click on the column header: add or remove metadata columns. The
    // layout is shared by every library source and saved by SourceTree.qml.
    Menu {
        id: columnMenu

        property var entries: []

        background: Rectangle {
            border.color: Theme.borderStrong
            border.width: 1
            color: Theme.surface2
            radius: Theme.radiusMd
        }
        padding: Theme.space1

        Instantiator {
            model: columnMenu.entries

            delegate: MenuItem {
                required property var modelData

                checkable: true
                checked: !modelData.hidden
                text: modelData.label

                onTriggered: root.setColumnHidden(modelData.column, !checked)
            }

            onObjectAdded: (index, object) => columnMenu.insertItem(index, object)
            onObjectRemoved: (index, object) => columnMenu.removeItem(object)
        }
    }
    Rectangle {
        id: dropMessage

        property alias text: dropMessageLabel.text

        Accessible.name: dropMessageLabel.text
        Accessible.role: Accessible.StaticText
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Theme.space3
        anchors.horizontalCenter: parent.horizontalCenter
        border.color: Theme.borderStrong
        border.width: 1
        color: Theme.surface3
        height: 26
        radius: Theme.radiusSm
        visible: dropMessageLabel.text.length > 0 && dropMessageTimer.running
        width: Math.min(root.width - 2 * Theme.space3, dropMessageLabel.implicitWidth + 2 * Theme.space3)
        z: 2

        Text {
            id: dropMessageLabel

            anchors.centerIn: parent
            color: Theme.textPrimary
            elide: Text.ElideRight
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeSm
            width: parent.width - 2 * Theme.space2
        }
    }
    Timer {
        id: dropMessageTimer

        interval: 4000
    }
    Rectangle {
        anchors.fill: view
        border.color: Theme.accent
        border.width: 2
        color: "transparent"
        visible: trackDropArea.containsDrag
        z: 1
    }
}
