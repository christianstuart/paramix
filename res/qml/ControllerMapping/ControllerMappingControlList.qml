import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts
import ".." as Skin
import "../Theme"

/// Searchable list of learnable controls.
///
/// Entries come from `ControllerMappingControlIndex`: one JS object per row of
/// the ControlPickerModel inventory, so filtering never instantiates a delegate
/// for a row that does not match.
Item {
    id: root

    property var entries: []
    property string filterText: ""
    property bool ready: false

    signal controlChosen(string group, string key)

    readonly property var filteredEntries: {
        const needle = root.filterText.trim().toLowerCase();
        if (needle.length === 0)
            return root.entries;
        const result = [];
        for (let i = 0; i < root.entries.length; ++i) {
            const entry = root.entries[i];
            if (entry.search.indexOf(needle) >= 0)
                result.push(entry);
        }
        return result;
    }

    Layout.fillHeight: true
    Layout.fillWidth: true

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.space2

        Skin.TextField {
            id: searchField

            Accessible.description: qsTr("Filters by title, description and category")
            Accessible.name: qsTr("Search controls")
            Accessible.role: Accessible.EditableText
            Layout.fillWidth: true
            placeholderText: qsTr("Search controls by name or category")
            ToolTip.delay: 500
            ToolTip.text: qsTr("Type to narrow the control list")
            ToolTip.visible: hovered

            onTextChanged: searchTimer.restart()

            Timer {
                id: searchTimer

                interval: 180

                onTriggered: root.filterText = searchField.text
            }
        }
        Text {
            Layout.fillWidth: true
            Accessible.role: Accessible.StaticText
            color: Theme.textMuted
            elide: Text.ElideRight
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeXs
            text: {
                if (!root.ready)
                    return qsTr("Loading the control list…");
                const total = root.entries.length;
                const shown = root.filteredEntries.length;
                return shown === total ? qsTr("%1 controls").arg(total) : qsTr("%1 of %2 controls").arg(shown).arg(total);
            }
            textFormat: Text.PlainText
        }
        Item {
            Layout.fillHeight: true
            Layout.fillWidth: true

            ListView {
                id: controlView

                Accessible.name: qsTr("Learnable controls")
                Accessible.role: Accessible.List
                anchors.fill: parent
                clip: true
                model: root.filteredEntries
                spacing: 1

                ScrollBar.vertical: ScrollBar {
                }
                delegate: Rectangle {
                    id: row

                    readonly property var entry: modelData

                    Accessible.description: row.entry.description.length > 0 ? row.entry.description + " (" + row.entry.configKey + ")" : row.entry.configKey
                    Accessible.name: row.entry.title
                    Accessible.role: Accessible.ListItem
                    color: rowMouse.containsMouse ? Theme.surfaceHover : "transparent"
                    height: Theme.space6 + Theme.space2
                    radius: Theme.radiusXs
                    width: controlView.width

                    ToolTip.delay: 700
                    ToolTip.text: row.entry.description.length > 0 ? row.entry.description + "\n" + row.entry.configKey : row.entry.configKey
                    ToolTip.visible: rowMouse.containsMouse

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.leftMargin: Theme.space2
                        anchors.rightMargin: Theme.space2
                        spacing: 0

                        Text {
                            Layout.fillWidth: true
                            color: Theme.textPrimary
                            elide: Text.ElideRight
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeMd
                            text: row.entry.title
                            textFormat: Text.PlainText
                        }
                        Text {
                            Layout.fillWidth: true
                            color: Theme.textMuted
                            elide: Text.ElideRight
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeXs
                            text: row.entry.category + " · " + (row.entry.description.length > 0 ? row.entry.description : row.entry.configKey)
                            textFormat: Text.PlainText
                        }
                    }
                    Rectangle {
                        anchors.bottom: parent.bottom
                        color: Theme.border
                        height: 1
                        width: parent.width
                    }
                    MouseArea {
                        id: rowMouse

                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        hoverEnabled: true

                        onClicked: root.controlChosen(row.entry.group, row.entry.key)
                    }
                }
            }
            Text {
                Accessible.role: Accessible.StaticText
                anchors.centerIn: parent
                color: Theme.textMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeSm
                text: root.ready ? qsTr("No controls match this search") : ""
                textFormat: Text.PlainText
                visible: root.ready && root.filteredEntries.length === 0
                width: parent.width - 2 * Theme.space4
                wrapMode: Text.WordWrap
            }
        }
    }
}
