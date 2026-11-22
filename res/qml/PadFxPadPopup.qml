pragma ComponentBehavior: Bound

import "." as Skin
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts
import "Theme"

/// Right-click editor for one Pad FX pad in a deck pane.
///
/// Holds the shared PadFxPadEditor used by the deck pane and the Pad FX
/// window, so the controls exist once for every deck.
Popup {
    id: root

    property string padFxGroup: ""
    property string presetGroup: "[PadFX]"
    property int bank: 1
    property int pad: 1
    /// The pad the popup points at; set through openFor().
    property Item anchor: null

    /// Comfortable editor width: the window's narrowest deck column, and wider
    /// than the widest row (title plus Close, about 140 px, and the editor).
    readonly property int popupWidth: Theme.padfxDeckMinWidth

    /// Open the editor for `padNumber` next to `item`. The x / y bindings below
    /// place it, so it also follows the editor's height once the loader fills.
    function openFor(item, padNumber) {
        root.anchor = item;
        root.pad = padNumber;
        root.open();
    }
    /// Keep the popup inside the window: right-aligned to the pad, below it
    /// when that fits, above it otherwise, and clamped to both edges. Qt 6.4 has
    /// no Popup.popupType, so both are computed in the pane's coordinates.
    function anchorX() {
        if (!root.anchor || !root.parent)
            return 0;
        const left = root.anchor.mapToItem(root.parent, 0, 0).x;
        const right = left + root.anchor.width - root.width;
        const win = root.anchor.Window.window;
        if (!win)
            return Math.max(Theme.space1, right);
        const origin = root.parent.mapToItem(null, 0, 0);
        const maxX = win.width - origin.x - root.width - Theme.space1;
        return Math.max(Theme.space1, Math.min(right, maxX));
    }
    function anchorY() {
        if (!root.anchor || !root.parent)
            return 0;
        const top = root.anchor.mapToItem(root.parent, 0, 0).y;
        const below = top + root.anchor.height + Theme.space1;
        const above = top - root.height - Theme.space1;
        const win = root.anchor.Window.window;
        if (!win)
            return below;
        const origin = root.parent.mapToItem(null, 0, 0);
        const bottom = win.height - origin.y;
        const y = below + root.height > bottom - Theme.space1 && above >= Theme.space1
                ? above : below;
        return Math.max(Theme.space1,
                        Math.min(y, Math.max(Theme.space1,
                                             bottom - root.height - Theme.space1)));
    }

    readonly property int editorHeight: editorLoader.height
    readonly property int maxHeight: {
        const win = root.anchor ? root.anchor.Window.window : null;
        return win ? Math.max(200, win.height - 2 * Theme.space1) : 600;
    }

    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    dim: false
    focus: true
    modal: false
    padding: Theme.space2
    width: root.popupWidth
    x: root.anchorX()
    y: root.anchorY()

    background: Rectangle {
        border.color: Theme.border
        border.width: 1
        color: Theme.surface1
        radius: Theme.radiusMd
    }
    // Only build the editor while the popup is open, so four decks do not
    // instantiate the parameter rows at startup. The Flickable keeps a tall
    // Beat FX editor usable when the window is short.
    contentItem: Flickable {
        id: editorFlickable

        boundsBehavior: Flickable.StopAtBounds
        clip: true
        contentHeight: editorLoader.height
        contentWidth: width
        implicitHeight: Math.min(root.editorHeight,
                root.maxHeight - 2 * root.padding)

        ScrollIndicator.vertical: ScrollIndicator {
        }

        Loader {
            id: editorLoader

            active: root.visible
            width: editorFlickable.width

            sourceComponent: ColumnLayout {
                spacing: Theme.space1

                RowLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.max(Theme.padfxHitHeight, titleText.implicitHeight)
                    spacing: Theme.space1

                    // Wraps instead of eliding if a larger type scale ever needs it.
                    Text {
                        id: titleText

                        Layout.fillWidth: true
                        color: Theme.textPrimary
                        elide: Text.ElideNone
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeMd
                        font.weight: Theme.fontWeightDemiBold
                        text: qsTr("Pad %1 - FX %2").arg(root.pad).arg(root.bank)
                        verticalAlignment: Text.AlignVCenter
                        wrapMode: Text.WordWrap
                    }
                    Skin.Button {
                        id: closeKey

                        Accessible.name: qsTr("Close pad editor")
                        Layout.preferredHeight: Theme.padfxHitHeight
                        implicitHeight: Theme.padfxHitHeight
                        text: qsTr("Close")

                        ToolTip.delay: 500
                        ToolTip.text: qsTr("Close pad editor")
                        ToolTip.visible: closeKey.hovered

                        onClicked: root.close()
                    }
                }
                Skin.PadFxPadEditor {
                    Layout.fillWidth: true
                    bank: root.bank
                    pad: root.pad
                    presetGroup: root.presetGroup
                }
            }
        }
    }
}
