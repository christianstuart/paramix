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
        // Keep the sort arrow on the sorted column while it moves.
        const sorted = horizontalHeader.sortingColumn;
        root.sidebar.moveColumn(from, to);
        if (sorted === from) {
            horizontalHeader.sortingColumn = to;
        } else if (from < sorted && to >= sorted) {
            horizontalHeader.sortingColumn = sorted - 1;
        } else if (from > sorted && to <= sorted) {
            horizontalHeader.sortingColumn = sorted + 1;
        }
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
    function sortByColumn(column) {
        // The Auto DJ queue does not advertise sorting: the processor always
        // plays row 0, so sorting would change the playback order. Sorting a
        // regular playlist still works.
        if (!view.model || !view.model.hasCapabilities(Mixxx.LibraryTrackListModel.Capability.Sorting))
            return;
        if (horizontalHeader.sortingColumn === column) {
            horizontalHeader.sortingOrder = horizontalHeader.sortingOrder === Qt.DescendingOrder ? Qt.AscendingOrder : Qt.DescendingOrder;
        } else {
            horizontalHeader.sortingColumn = column;
            horizontalHeader.sortingOrder = Qt.AscendingOrder;
        }
        view.model.sort(horizontalHeader.sortingColumn, horizontalHeader.sortingOrder);
    }
    function openColumnMenu(x, y) {
        columnMenu.model = root.columnMenuEntries();
        columnMenu.x = Math.max(0, Math.min(x, root.width - columnMenu.width));
        columnMenu.y = Math.max(0, Math.min(y, root.height - columnMenu.height));
        columnMenu.open();
    }
    function columnMenuEntries() {
        const entries = [];
        if (!root.model)
            return entries;
        const columns = root.model.columns;
        for (let i = 0; i < columns.length; ++i) {
            // The cover thumbnail has no label and is not selectable.
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

            /// True while this column is being dragged to another slot.
            property bool reordering: false

            Accessible.name: qsTr("Sort by %1").arg(display)
            Accessible.role: Accessible.Button
            implicitHeight: 32
            implicitWidth: columnName.contentWidth + Theme.space2
            opacity: column.reordering ? 0.45 : 1

            HoverHandler {
                id: columnHover
            }
            TapHandler {
                id: columnTap

                acceptedButtons: Qt.LeftButton | Qt.RightButton

                onTapped: (eventPoint, button) => {
                    if (button === Qt.RightButton) {
                        const point = column.mapToItem(root, eventPoint.position.x, eventPoint.position.y);
                        root.openColumnMenu(point.x, point.y);
                    } else {
                        root.sortByColumn(index);
                    }
                }
            }
            DragHandler {
                id: columnDrag

                enabled: !columnResizeHandler.pressed && !columnResizeHandler.containsMouse
                target: null
                xAxis.enabled: true
                yAxis.enabled: false

                onActiveChanged: {
                    if (active) {
                        column.reordering = true;
                        const point = column.mapToItem(root, centroid.position.x, 0);
                        root.updateColumnDrop(point.x);
                    } else if (column.reordering) {
                        column.reordering = false;
                        const from = index;
                        const slot = root.columnDropSlot;
                        // The reorder rebuilds the header delegates, so move out
                        // of the finished gesture before the view is laid out.
                        Qt.callLater(() => root.moveColumnToSlot(from, slot));
                    }
                }
                onTranslationChanged: {
                    if (active) {
                        const point = column.mapToItem(root, centroid.position.x, 0);
                        root.updateColumnDrop(point.x);
                    }
                }
            }
            Text {
                id: columnName

                anchors.fill: parent
                anchors.leftMargin: Theme.space3
                anchors.rightMargin: Theme.space4
                color: columnHover.hovered ? Theme.textPrimary : Theme.textSecondary
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
            Item {
                id: columnResizer

                anchors.bottom: parent.bottom
                anchors.right: parent.right
                anchors.top: parent.top
                visible: column.width > 0
                width: 7

                Rectangle {
                    anchors.bottom: parent.bottom
                    anchors.right: parent.right
                    anchors.top: parent.top
                    color: columnResizeHandler.containsMouse ? Theme.accent : Theme.border
                    width: 1
                }
                MouseArea {
                    id: columnResizeHandler

                    property real startPointerX: 0
                    property real startWidth: 0

                    anchors.fill: parent
                    cursorShape: Qt.SizeHorCursor
                    hoverEnabled: true
                    preventStealing: true

                    onPositionChanged: {
                        if (!pressed)
                            return;
                        const pointerX = columnResizer.mapToItem(root, mouse.x, 0).x;
                        const dx = pointerX - startPointerX;
                        if (Math.abs(dx) < 1)
                            return;
                        const columnDef = view.model ? view.model.columns[index] : null;
                        if (!columnDef)
                            return;
                        const width = Math.max(root.minimumColumnWidth, Math.round(startWidth + dx));
                        if (columnDef.preferredWidth === width)
                            return;
                        columnDef.preferredWidth = width;
                        view.updateColumnSize();
                        view.forceLayout();
                    }
                    onPressed: {
                        startWidth = view.columnWidthProvider(index);
                        startPointerX = columnResizer.mapToItem(root, mouse.x, 0).x;
                    }
                }
            }
        }
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
            required property url cover_art
            required property color decoration
            required property var display
            required property string file_url
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

                property var capabilities: root.model ? root.model.getCapabilities() : Mixxx.LibraryTrackListModel.Capability.None
                property url cover_art: item.cover_art
                property color decoration: item.decoration
                property var display: item.display
                property url file_url: item.file_url
                /// `has_stems` from the track list model, mirrored for the
                /// column delegates (see `SourceTree.qml`).
                property bool has_stems: item.has_stems
                property int row: item.row
                property bool selected: item.selected
                /// True for the single cell that owns the per-row stem badge.
                property bool stemBadgeVisible: item.has_stems && item.column === root.titleColumnIndex
                property var tableView: view
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
            Item {
                id: stemBadge

                Accessible.ignored: true
                anchors.verticalCenter: parent.verticalCenter
                height: 12
                visible: loader.stemBadgeVisible
                width: 12
                x: Theme.space1

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
    // Right-click on the column header: add or remove metadata columns. A plain
    // Popup with text rows is used instead of a Menu, because the Menu items
    // relied on palette styling that did not render in this skin.
    Popup {
        id: columnMenu

        property var model: []

        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        height: Math.min(420, columnMenu.model.length * 29 + 2 * padding)
        padding: Theme.space1
        parent: root
        width: 260

        background: Rectangle {
            border.color: Theme.borderStrong
            border.width: 1
            color: Theme.surface2
            radius: Theme.radiusMd
        }

        ListView {
            id: list

            anchors.fill: parent
            clip: true
            model: columnMenu.model
            spacing: 1

            delegate: Item {
                id: menuRow

                required property var modelData

                height: 28
                width: ListView.view.width

                Rectangle {
                    anchors.fill: parent
                    color: rowMouse.containsMouse ? Theme.surface3 : "transparent"
                    radius: Theme.radiusXs
                }
                Rectangle {
                    anchors.left: parent.left
                    anchors.leftMargin: Theme.space2
                    anchors.verticalCenter: parent.verticalCenter
                    border.color: menuRow.modelData.hidden ? Theme.borderStrong : Theme.accent
                    border.width: 1
                    color: menuRow.modelData.hidden ? "transparent" : Theme.accent
                    height: 12
                    radius: Theme.radiusXs
                    width: 12
                }
                Text {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.space2 + 16
                    anchors.rightMargin: Theme.space2
                    color: Theme.textPrimary
                    elide: Text.ElideRight
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSm
                    text: menuRow.modelData.label
                    verticalAlignment: Text.AlignVCenter
                }
                MouseArea {
                    id: rowMouse

                    anchors.fill: parent
                    hoverEnabled: true

                    onClicked: {
                        root.setColumnHidden(menuRow.modelData.column, !menuRow.modelData.hidden);
                        columnMenu.close();
                    }
                }
            }
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
