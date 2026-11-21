pragma ComponentBehavior: Bound

import "." as Skin
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts
import QtQuick.Window 2.15
import "Theme"

/// Dedicated Pad FX workspace. Closed by default; opened from the View menu.
///
/// One column per deck, each with its own A-D bank. The number of columns
/// follows the main window (`deckCount`): two decks show two wide columns, four
/// decks show four columns when the window is wide enough and otherwise page
/// two decks at a time, so stem chip labels never get squeezed.
Window {
    id: root

    /// Set by MainWindow from `show4decks ? 4 : 2`.
    property int deckCount: 4
    /// Presentation-only page index; never writes a Pad FX control.
    property int page: 0

    readonly property bool canShowFour: (root.width - 2 * Theme.space2)
            >= 4 * Theme.padfxDeckMinWidth + 3 * Theme.space2
    readonly property int columnsPerPage: root.deckCount <= 2
            ? 2
            : (root.canShowFour ? 4 : 2)
    readonly property int pageCount: Math.max(1, Math.ceil(root.deckCount / root.columnsPerPage))
    readonly property int firstDeckIndex: root.page * root.columnsPerPage
    readonly property int visibleColumns: Math.max(0,
                                                   Math.min(root.columnsPerPage,
                                                            root.deckCount - root.firstDeckIndex))

    color: Theme.surface0
    height: 720
    minimumHeight: 480
    minimumWidth: 760
    title: qsTr("Pad FX")
    visible: false
    width: 1240

    onDeckCountChanged: root.page = 0
    onPageCountChanged: {
        if (root.page >= root.pageCount)
            root.page = 0;
    }
    onWidthChanged: {
        if (root.page >= root.pageCount)
            root.page = 0;
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.space2
        spacing: Theme.space2

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.space6
            spacing: Theme.space2

            Text {
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeLg
                font.weight: Theme.fontWeightDemiBold
                text: qsTr("Pad FX")
                verticalAlignment: Text.AlignVCenter
            }
            Text {
                Layout.fillWidth: true
                color: Theme.textMuted
                elide: Text.ElideNone
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeSm
                text: qsTr("Per-stem performance effects")
                verticalAlignment: Text.AlignVCenter
            }
            Skin.Button {
                id: pageKey

                Accessible.name: root.page === 0
                                ? qsTr("Show decks 3 and 4")
                                : qsTr("Show decks 1 and 2")
                Layout.alignment: Qt.AlignVCenter
                implicitHeight: Theme.padfxHitHeight
                text: root.page === 0 ? qsTr("Decks 1-2") : qsTr("Decks 3-4")
                visible: root.pageCount > 1

                ToolTip.delay: 500
                ToolTip.text: qsTr("Switch to the other deck page")
                ToolTip.visible: pageKey.hovered

                onClicked: root.page = (root.page + 1) % root.pageCount
            }
            Skin.Button {
                id: closeKey

                Accessible.name: qsTr("Close Pad FX")
                Layout.alignment: Qt.AlignVCenter
                implicitHeight: Theme.padfxControlHeight
                text: qsTr("Close")

                ToolTip.delay: 500
                ToolTip.text: qsTr("Close Pad FX")
                ToolTip.visible: closeKey.hovered

                onClicked: root.close()
            }
        }
        RowLayout {
            Layout.fillHeight: true
            Layout.fillWidth: true
            spacing: Theme.space2

            Repeater {
                model: root.visibleColumns

                delegate: Skin.PadFxDeckPanel {
                    required property int index

                    Layout.fillHeight: true
                    Layout.fillWidth: true
                    group: "[Channel" + (root.firstDeckIndex + index + 1) + "]"
                }
            }
        }
    }
}
