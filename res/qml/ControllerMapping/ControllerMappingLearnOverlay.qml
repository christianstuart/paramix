import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts
import ".." as Skin
import "../Theme"

/// Full-surface click catcher for learn-by-click, installed into the main
/// window while learn mode is active.
///
/// QML items do not expose their `Mixxx.ControlProxy` generically. This overlay
/// therefore resolves the *item* under the press and walks its parent chain
/// looking for a `group` / `key` pair. The mixed-style controls in `res/qml`
/// carry that pair (ControlButton, ControlFader, ControlKnob, ControlMiniKnob),
/// so knobs, faders and buttons resolve. Items that only pass a key to a
/// behaviour, or that build the control in a nested proxy (hotcues, for
/// example), do not resolve; the window then falls back to the search list.
Item {
    id: root

    property bool active: false
    property var controlEntries: []
    property string feedback: ""
    property Item hostItem: null
    property string instruction: ""

    signal cancelled()
    signal controlPicked(string group, string key)
    signal unresolvedPicked()

    focus: root.visible
    height: parent ? parent.height : 0
    parent: root.hostItem
    visible: root.active && parent !== null
    width: parent ? parent.width : 0
    x: 0
    y: 0
    z: 100000

    Keys.onEscapePressed: root.cancelled()

    onActiveChanged: {
        if (root.active) {
            root.feedback = "";
            // The window is raised right after learn mode is turned on, so give
            // it the event loop turn before asking for keyboard focus.
            Qt.callLater(function () {
                root.forceActiveFocus();
            });
        }
    }

    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, 0.25)
    }

    // Sits under the banner so the Cancel key stays clickable.
    MouseArea {
        id: catcher

        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        cursorShape: Qt.CrossCursor

        onClicked: root.resolveAt(mouse.x, mouse.y)
    }
    Rectangle {
        id: banner

        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: Theme.space5
        border.color: Theme.borderStrong
        border.width: 1
        color: Theme.surface2
        implicitHeight: bannerLayout.implicitHeight + 2 * Theme.space3
        implicitWidth: Math.min(parent.width - 2 * Theme.space5, bannerLayout.implicitWidth + 2 * Theme.space3)
        radius: Theme.radiusMd

        Accessible.name: root.feedback.length > 0 ? root.feedback : root.instruction
        Accessible.role: Accessible.Grouping

        RowLayout {
            id: bannerLayout

            anchors.fill: parent
            anchors.margins: Theme.space3
            spacing: Theme.space3

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0

                Text {
                    Layout.fillWidth: true
                    Accessible.role: Accessible.StaticText
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeMd
                    font.weight: Theme.fontWeightDemiBold
                    text: root.instruction
                    textFormat: Text.PlainText
                    wrapMode: Text.WordWrap
                }
                Text {
                    Layout.fillWidth: true
                    Accessible.role: Accessible.StaticText
                    color: root.feedback.length > 0 ? Theme.warning : Theme.textMuted
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeXs
                    text: root.feedback.length > 0 ? root.feedback : qsTr("Knobs, faders and buttons resolve. Everything else: use the search list in the mapping window. Esc cancels.")
                    textFormat: Text.PlainText
                    wrapMode: Text.WordWrap
                }
            }
            Skin.Button {
                id: cancelKey

                Accessible.name: qsTr("Cancel learning by click")
                Layout.alignment: Qt.AlignVCenter
                text: qsTr("Cancel")

                ToolTip.delay: 500
                ToolTip.text: qsTr("Leave learn mode")
                ToolTip.visible: cancelKey.hovered

                onClicked: root.cancelled()
            }
        }
    }

    function resolveAt(x, y) {
        const host = root.parent;
        if (!host)
            return;
        // The overlay would be the topmost child at every point, so hide it
        // for the duration of the hit test. Visibility changes take effect
        // immediately for childAt().
        root.visible = false;
        let pick = null;
        try {
            pick = root.pickAt(host, x, y);
        } finally {
            root.visible = root.active && root.parent !== null;
        }
        if (pick) {
            root.feedback = "";
            root.controlPicked(pick.group, pick.key);
        } else {
            root.feedback = qsTr("No mappable control under the cursor. Use the search list in the mapping window.");
            root.unresolvedPicked();
        }
    }

    function pickAt(host, x, y) {
        let item = root.deepestItem(host, x, y);
        let guard = 0;
        while (item && item !== host && guard++ < 500) {
            if (root.isMappableItem(item) && root.isKnownControl(item.group, item.key))
                return {
                    "group": item.group,
                    "key": item.key
                };
            item = item.parent;
        }
        return null;
    }

    function deepestItem(item, x, y) {
        let current = item;
        let px = x;
        let py = y;
        let guard = 0;
        while (current && guard++ < 200) {
            const child = current.childAt(px, py);
            if (!child)
                return current;
            const local = child.mapFromItem(current, px, py);
            current = child;
            px = local.x;
            py = local.y;
        }
        return current;
    }

    function isMappableItem(item) {
        if (!item || typeof item.group !== "string" || typeof item.key !== "string")
            return false;
        if (item.group.length < 3 || item.key.length === 0)
            return false;
        if (item.group.charAt(0) !== "[")
            return false;
        // A real interactive control, not a read-only meter or a container that
        // happens to forward a group. Buttons expose `pressed`, knobs and
        // faders expose `value`.
        return typeof item.pressed === "boolean" || typeof item.value === "number";
    }

    function isKnownControl(group, key) {
        const entries = root.controlEntries;
        if (!entries || entries.length === 0)
            return true;
        for (let i = 0; i < entries.length; ++i) {
            if (entries[i].group === group && entries[i].key === key)
                return true;
        }
        return false;
    }
}
