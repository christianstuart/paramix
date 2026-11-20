import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts
import Mixxx 1.0 as Mixxx
import ".." as Skin
import "../Theme"

/// Controls learned in this session (`Mixxx.ControllerLearning.pendingMappings`).
///
/// The backend has no redo stack: "Undo last" removes the newest row, which also
/// drops its mapping from the working mapping. Delete removes one control's
/// mappings and its row.
Item {
    id: root

    readonly property alias pendingCount: pendingView.count
    property bool learning: false

    Layout.fillHeight: true
    Layout.fillWidth: true

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.space2

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space2

            Text {
                Layout.fillWidth: true
                Accessible.role: Accessible.StaticText
                color: Theme.textPrimary
                elide: Text.ElideRight
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeSm
                font.weight: Theme.fontWeightDemiBold
                text: qsTr("This session")
                textFormat: Text.PlainText
            }
            Skin.Button {
                id: undoKey

                Accessible.name: qsTr("Undo the last learned mapping")
                enabled: root.pendingCount > 0
                text: qsTr("Undo last")

                ToolTip.delay: 500
                ToolTip.text: qsTr("Remove the most recently learned control from the working mapping")
                ToolTip.visible: undoKey.hovered

                onClicked: Mixxx.ControllerLearning.removeAt(root.pendingCount - 1)
            }
        }
        Text {
            Layout.fillWidth: true
            Accessible.role: Accessible.StaticText
            color: Theme.textMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeXs
            text: root.learning ? qsTr("Nothing learned yet. Move a hardware control, then choose a target.") : qsTr("Nothing learned in this session.")
            textFormat: Text.PlainText
            visible: root.pendingCount === 0
            wrapMode: Text.WordWrap
        }
        ListView {
            id: pendingView

            Accessible.name: qsTr("Controls learned in this session")
            Accessible.role: Accessible.List
            Layout.fillHeight: true
            Layout.fillWidth: true
            clip: true
            model: Mixxx.ControllerLearning.pendingMappings
            spacing: Theme.space1
            visible: root.pendingCount > 0

            ScrollBar.vertical: ScrollBar {
            }
            delegate: Rectangle {
                id: pendingRow

                readonly property string rowTitle: title.length > 0 ? title : configKeyString
                readonly property var optionList: optionNames

                Accessible.description: configKeyString
                Accessible.name: pendingRow.rowTitle
                Accessible.role: Accessible.ListItem
                border.color: Theme.border
                border.width: 1
                color: Theme.surface1
                height: Theme.space7 + Theme.space2
                radius: Theme.radiusSm
                width: pendingView.width

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.space2
                    spacing: Theme.space2

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0

                        Text {
                            Layout.fillWidth: true
                            color: Theme.textPrimary
                            elide: Text.ElideRight
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeSm
                            text: pendingRow.rowTitle
                            textFormat: Text.PlainText
                        }
                        Text {
                            Layout.fillWidth: true
                            color: Theme.textMuted
                            elide: Text.ElideRight
                            font.family: Theme.fontFamilyMono
                            font.pixelSize: Theme.fontSizeXs
                            text: configKeyString + " · " + message
                            textFormat: Text.PlainText
                        }
                        Text {
                            Layout.fillWidth: true
                            color: Theme.textSecondary
                            elide: Text.ElideRight
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeXs
                            text: pendingRow.optionList.length > 0 ? pendingRow.optionList.join(", ") : qsTr("No options")
                            textFormat: Text.PlainText
                        }
                    }
                    Skin.Button {
                        Accessible.name: qsTr("Delete the mapping for %1").arg(pendingRow.rowTitle)
                        Layout.alignment: Qt.AlignVCenter
                        text: qsTr("Delete")

                        ToolTip.delay: 500
                        ToolTip.text: qsTr("Remove this control from the working mapping")
                        ToolTip.visible: hovered

                        onClicked: Mixxx.ControllerLearning.removeAt(index)
                    }
                }
            }
        }
    }
}
