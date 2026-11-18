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

    required property var model
    /// Rows are uniform, so keyboard paging and scrolling can be computed
    /// without a row height provider.
    readonly property int rowHeight: 28
    /// Capabilities are per model, not per cell. Resolve them once here
    /// instead of invoking the model from every delegate.
    readonly property var capabilities: model ? model.getCapabilities() : Mixxx.LibraryTrackListModel.Capability.None
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

    /// Row the context menu was last opened for. Used to restore a usable
    /// selection after an action changed the underlying model.
    property int contextRow: -1
    /// Set while a pointer gesture started with a selection modifier. The
    /// column delegates load Track.qml, whose own tap handler calls
    /// selectionModel.selectRow() (a plain replacement) when the button is
    /// released. Without this guard that call would wipe the multi-selection
    /// the modified click builds. It is recomputed on every press, so a stale
    /// value can never leak into the next gesture.
    property bool suppressPlainSelect: false

    color: Theme.surface0

    function hasSelectionModifier(modifiers) {
        return (modifiers & (Qt.ShiftModifier | Qt.ControlModifier | Qt.MetaModifier)) !== 0;
    }
    function selectRow(row) {
        if (root.suppressPlainSelect)
            return;
        view.selectionModel.selectSingleton(row);
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

            Accessible.name: qsTr("Sort by %1").arg(display)
            Accessible.role: Accessible.Button
            implicitHeight: 32
            implicitWidth: columnName.contentWidth + Theme.space2

            MouseArea {
                id: columnMouseHandler

                acceptedButtons: Qt.LeftButton
                anchors.fill: parent
                hoverEnabled: true

                onClicked: {
                    if (horizontalHeader.sortingColumn == index) {
                        horizontalHeader.sortingOrder = horizontalHeader.sortingOrder == Qt.DescendingOrder ? Qt.AscendingOrder : Qt.DescendingOrder;
                    } else {
                        horizontalHeader.sortingColumn = index;
                        horizontalHeader.sortingOrder = Qt.AscendingOrder;
                    }
                    view.model.sort(horizontalHeader.sortingColumn, horizontalHeader.sortingOrder);
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
    TableView {
        id: view

        property int dynamicColumnCount: 0
        property int usedWidth: 0
        readonly property int pageRowCount: Math.max(1, Math.floor(height / root.rowHeight))

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
            return span * (view.width - view.usedWidth) / view.dynamicColumnCount;
        }
        keyNavigationEnabled: false
        model: root.model
        pointerNavigationEnabled: false
        reuseItems: true

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
            required property int row
            required property bool selected
            required property var track

            /// Keyboard modifiers of the press that started the current
            /// gesture. Captured on press because the tapped signal only
            /// carries the event point and the button.
            property int pressModifiers: Qt.NoModifier

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

            function modelRowCount() {
                return this.model ? this.model.rowCount() : 0;
            }
            function normalizeRow(row) {
                const rowCount = this.modelRowCount();
                if (rowCount === 0)
                    return -1;
                return Mixxx.MathUtils.positiveModulo(row, rowCount);
            }
            function currentRow() {
                const index = this.currentIndex;
                return index ? index.row : -1;
            }
            function effectiveCurrentRow() {
                const row = this.currentRow();
                if (row >= 0)
                    return row;
                const rows = this.selectedRows();
                return rows.length > 0 ? rows[0] : -1;
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
            function currentTrackUrl() {
                const row = this.effectiveCurrentRow();
                if (row < 0)
                    return null;
                return this.model.getUrl(row);
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
            function clearSelectionAndCurrent() {
                this.clear();
                this.anchorRow = -1;
            }
            function setCurrentRow(row) {
                const newRow = this.normalizeRow(row);
                if (newRow < 0)
                    return;
                this.setCurrentIndex(this.model.index(newRow, 0), ItemSelectionModel.NoUpdate);
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
            /// Moves the current row by `offset` rows and selects it, unless
            /// `extend` is set, in which case the selection is grown from the
            /// anchor like Shift+click.
            function moveSelectionVertical(offset, extend) {
                if (offset === 0)
                    return;
                const base = Math.max(0, this.effectiveCurrentRow());
                this.moveToRow(base + offset, extend);
            }
            function moveSelectionTo(row, extend) {
                this.moveToRow(row, extend);
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
                view.loadSelectedTrackIntoNextAvailableDeck(false);
                break;
            default:
                // Delete / Shift+Delete are not wired: the track menu proxy
                // only exposes invoke(index) into the serialized WTrackMenu,
                // whose Remove/Hide actions carry no stable identifier.
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
    }
    Connections {
        target: Mixxx.LibraryTrackMenu

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
}
