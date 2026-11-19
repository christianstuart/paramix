import QtQuick
import QtQuick.Controls 2.15
import QtQuick.Layouts
import ".." as Skin
import "../Theme"

/// Shows or hides the library sources of the sidebar. Hidden sources keep
/// working; only the source tree skips them, so this is also the way back.
Dialog {
    id: root

    property var sidebar: null
    /// Snapshot of QmlSidebarModelProxy::sources(), refreshed on every change.
    property var items: []

    function refresh() {
        items = root.sidebar ? root.sidebar.sources() : [];
    }

    // The header is drawn inside contentItem; the title still names the dialog
    // for assistive technology.
    title: qsTr("Library sources")
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    header: null
    modal: true
    padding: Theme.space3
    width: Math.min(400, parent ? parent.width - 2 * Theme.space3 : 400)

    onAboutToShow: {
        // Assign instead of binding: y depends on the height the content
        // settles at, and a binding there loops while the popup sizes itself.
        if (parent) {
            x = Math.round((parent.width - width) / 2);
            y = Math.round((parent.height - height) / 2);
        }
    }
    onOpened: root.refresh()

    Connections {
        function onSourcesChanged() {
            root.refresh();
        }

        target: root.sidebar
    }

    background: Rectangle {
        border.color: Theme.border
        border.width: 1
        color: Theme.surface2
        radius: Theme.radiusMd
    }
    contentItem: ColumnLayout {
        spacing: Theme.space2

        Label {
            Layout.fillWidth: true
            color: Theme.textPrimary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeMd
            font.weight: Theme.fontWeightDemiBold
            text: qsTr("Library sources")
        }
        Label {
            Layout.fillWidth: true
            color: Theme.textMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeSm
            text: qsTr("Untick a source to remove it from the sidebar.")
            wrapMode: Text.WordWrap
        }
        ListView {
            id: sourceList

            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(contentHeight, 320)
            clip: true
            model: root.items
            spacing: Theme.space1

            delegate: Item {
                height: 30
                width: ListView.view.width

                RowLayout {
                    anchors.fill: parent
                    spacing: Theme.space2

                    CheckBox {
                        // `modelData` is one entry of sources(): id, title,
                        // icon, available, hidden.
                        checked: !modelData.hidden
                        palette.highlight: Theme.accent
                        palette.text: Theme.textPrimary
                        palette.windowText: Theme.textPrimary
                        text: modelData.title

                        onClicked: root.sidebar.setSourceHidden(modelData.id, !checked)
                    }
                    Label {
                        Layout.fillWidth: true
                        color: Theme.textMuted
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeXs
                        text: modelData.available ? "" : qsTr("Not found on this computer")
                        visible: text !== ""
                    }
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space2

            Skin.Button {
                enabled: root.sidebar !== null
                text: qsTr("Show all")
                onClicked: root.sidebar.showAllSources()
            }
            Item {
                Layout.fillWidth: true
            }
            Skin.Button {
                text: qsTr("Close")
                onClicked: root.close()
            }
        }
    }
}
