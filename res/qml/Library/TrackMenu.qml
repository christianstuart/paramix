import Mixxx 1.0 as Mixxx
import QtQuick
import QtQuick.Controls 2.15
import "../Theme"

/// Styled renderer for the classic Mixxx track menu.
///
/// The action tree comes from Mixxx.LibraryTrackMenu, which builds the real
/// WTrackMenu for the current selection. That keeps every classic action
/// (stems, AutoDJ queue, playlists, crates, BPM, metadata, clear, analyze,
/// file operations, properties, ...) working without duplicating any of it in
/// QML. This file only provides the Paramix look and forwards clicks.
Menu {
    id: root

    property var nodes: []

    function popupFor(model, rows) {
        root.nodes = Mixxx.LibraryTrackMenu.open(model, rows);
        if (root.nodes.length === 0) {
            return;
        }
        root.rebuild();
        root.popup();
    }
    function rebuild() {
        while (root.count > 0) {
            root.removeItem(root.itemAt(0));
        }
        root.buildInto(root, root.nodes);
    }
    function buildInto(menu, nodes) {
        for (let i = 0; i < nodes.length; ++i) {
            const node = nodes[i];
            if (node.kind === "separator") {
                menu.addItem(separatorComponent.createObject(menu));
                continue;
            }
            if (node.children && node.children.length > 0) {
                const subMenu = subMenuComponent.createObject(menu);
                subMenu.title = node.text;
                subMenu.enabled = node.enabled;
                subMenu.buildInto(subMenu, node.children);
                menu.addMenu(subMenu);
                continue;
            }
            if (node.kind === "rating") {
                menu.addMenu(root.createRatingMenu(menu, node));
                continue;
            }
            if (node.kind === "color") {
                menu.addMenu(root.createColorMenu(menu, node));
                continue;
            }
            const item = actionItemComponent.createObject(menu);
            item.nodeKind = node.kind;
            item.nodeIndex = node.index;
            item.nodeCheckIndex = node.checkIndex;
            item.enabled = node.enabled;
            item.checkable = node.checkable;
            item.checked = node.checkState === 2 ? true : node.checked;
            item.text = node.text;
            item.nodeShortcut = node.shortcut;
            menu.addItem(item);
        }
    }
    function createRatingMenu(parentMenu, node) {
        const subMenu = subMenuComponent.createObject(parentMenu);
        subMenu.title = qsTr("Rating");
        subMenu.enabled = node.enabled;
        for (let rating = 0; rating <= 5; ++rating) {
            const item = actionItemComponent.createObject(subMenu);
            item.nodeKind = "ratingValue";
            item.nodeIndex = rating;
            item.checkable = true;
            item.checked = node.rating === rating;
            item.text = rating === 0 ? qsTr("No rating") : "★".repeat(rating);
            subMenu.addItem(item);
        }
        return subMenu;
    }
    function createColorMenu(parentMenu, node) {
        const subMenu = subMenuComponent.createObject(parentMenu);
        subMenu.title = qsTr("Select Color");
        const palette = Mixxx.Config.trackColorPalette;
        for (let i = 0; i < palette.length; ++i) {
            const item = colorItemComponent.createObject(subMenu);
            item.nodeKind = "colorValue";
            item.nodeColor = String(palette[i]);
            item.text = item.nodeColor;
            item.checked = node.color === item.nodeColor;
            item.checkable = true;
            subMenu.addItem(item);
        }
        const noneItem = actionItemComponent.createObject(subMenu);
        noneItem.nodeKind = "colorValue";
        noneItem.nodeColor = "";
        noneItem.text = qsTr("No color");
        noneItem.checkable = true;
        noneItem.checked = node.color.length === 0;
        subMenu.addItem(noneItem);
        return subMenu;
    }
    function triggerNode(item) {
        switch (item.nodeKind) {
        case "ratingValue":
            Mixxx.LibraryTrackMenu.setRating(item.nodeIndex);
            break;
        case "colorValue":
            Mixxx.LibraryTrackMenu.setColor(item.nodeColor);
            break;
        case "check":
            Mixxx.LibraryTrackMenu.toggleCheckItem(item.nodeCheckIndex);
            break;
        default:
            Mixxx.LibraryTrackMenu.invoke(item.nodeIndex);
            break;
        }
    }

    title: qsTr("Track")

    background: Rectangle {
        // The Basic style derives Menu.implicitWidth from the background's
        // implicit width. Without it the popup collapses to zero width and is
        // invisible, because the content ListView only reports a height.
        // 320 px fits the longest classic entry (hotcue sorting) and leaves a
        // right-aligned column for the shortcuts.
        implicitWidth: 320
        border.color: Theme.border
        border.width: 1
        color: Theme.surface2
        radius: Theme.radiusMd
    }
    palette.buttonText: Theme.textPrimary
    palette.highlight: Theme.accent
    palette.highlightedText: Theme.textOnAccent
    palette.mid: Theme.textMuted
    palette.text: Theme.textPrimary
    palette.window: Theme.surface2
    palette.windowText: Theme.textPrimary

    Component {
        id: separatorComponent

        MenuSeparator {
        }
    }
    Component {
        id: actionItemComponent

        MenuItem {
            id: actionItem

            property string nodeColor: ""
            property int nodeCheckIndex: -1
            property int nodeIndex: -1
            property string nodeKind: "action"
            property string nodeShortcut: ""

            // Label and shortcut as two columns: gluing the shortcut into the
            // label with spaces pushed long entries past the menu edge and had
            // no right-aligned column.
            contentItem: Item {
                Text {
                    id: actionLabel

                    anchors.left: parent.left
                    anchors.right: shortcutLabel.left
                    anchors.rightMargin: actionItem.nodeShortcut.length > 0 ? 24 : 8
                    anchors.verticalCenter: parent.verticalCenter
                    color: actionItem.enabled ? Theme.textPrimary : Theme.textMuted
                    elide: Text.ElideRight
                    font: actionItem.font
                    text: actionItem.text
                }
                Text {
                    id: shortcutLabel

                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    color: Theme.textMuted
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeXs
                    text: actionItem.nodeShortcut
                }
            }
            onTriggered: root.triggerNode(actionItem)
        }
    }
    Component {
        id: colorItemComponent

        MenuItem {
            id: colorItem

            property string nodeColor: ""
            property int nodeCheckIndex: -1
            property int nodeIndex: -1
            property string nodeKind: "colorValue"

            contentItem: Row {
                spacing: 6

                Rectangle {
                    border.color: Theme.borderStrong
                    border.width: 1
                    color: colorItem.nodeColor.length > 0 ? colorItem.nodeColor : "transparent"
                    height: 14
                    radius: 3
                    width: 14
                }
                Text {
                    color: colorItem.enabled ? Theme.textPrimary : Theme.textMuted
                    text: colorItem.text
                    verticalAlignment: Text.AlignVCenter
                }
            }
            onTriggered: root.triggerNode(colorItem)
        }
    }
    Component {
        id: subMenuComponent

        Menu {
            id: subMenu

            function buildInto(menu, nodes) {
                root.buildInto(menu, nodes);
            }

            background: Rectangle {
                implicitWidth: 320
                border.color: Theme.border
                border.width: 1
                color: Theme.surface2
                radius: Theme.radiusMd
            }
            palette.buttonText: Theme.textPrimary
            palette.highlight: Theme.accent
            palette.highlightedText: Theme.textOnAccent
            palette.mid: Theme.textMuted
            palette.text: Theme.textPrimary
            palette.window: Theme.surface2
            palette.windowText: Theme.textPrimary
        }
    }
}
