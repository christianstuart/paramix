import ".." as Skin
import QtQuick 2.12
import QtQuick.Layouts
import "../Theme"

/// Horizontal CUE / MOVE / GRID selector above the deck pane.
///
/// The row is one tab high and spans the pane width, so the deck's transport
/// band stays the same height while the old 34 px vertical column is gone. The
/// active pane is filled by the accent; the grid lock badge stays on the GRID
/// tab so the state is visible from every pane.
RowLayout {
    id: root

    /// 0 = Move (beatjump), 1 = Cue (hotcues), 2 = Grid (beatgrid).
    property int activePane: 1
    /// Lights a small badge on the GRID tab while the BPM is locked.
    property bool gridLocked: false

    signal selected(int pane)

    objectName: "deckPaneTabs"
    spacing: Theme.space1

    Repeater {
        // Display order: Cue first (the default pane), then Move, Grid.
        // The `pane` values are the persisted Deck.Pane enum, so reordering the
        // list does not invalidate a saved selection.
        model: [
            {
                "label": qsTr("Cue"),
                "pane": 1,
                "hint": qsTr("Show hotcues")
            },
            {
                "label": qsTr("Move"),
                "pane": 0,
                "hint": qsTr("Show beatjump controls")
            },
            {
                "label": qsTr("Grid"),
                "pane": 2,
                "hint": qsTr("Show beatgrid controls")
            }
        ]

        delegate: Skin.Button {
            id: tab

            required property var modelData
            readonly property bool current: root.activePane === modelData.pane

            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.minimumHeight: Theme.deckPaneTabHeight
            highlight: tab.current
            normalColor: Theme.textMuted

            Accessible.checkable: true
            Accessible.checked: tab.current
            Accessible.description: tab.modelData.hint
            Accessible.name: qsTr("Show %1 pane").arg(tab.modelData.label)
            Accessible.role: Accessible.RadioButton

            contentItem: Text {
                color: tab.activeState ? tab.activeColor : tab.hovered ? Theme.textPrimary : Theme.textMuted
                font.capitalization: Font.AllUppercase
                font.family: Theme.fontFamily
                font.letterSpacing: Theme.letterSpacingCaps
                font.pixelSize: Math.min(Theme.fontSizeXs, Theme.deckPaneTabHeight - 4)
                font.weight: Theme.fontWeightDemiBold
                horizontalAlignment: Text.AlignHCenter
                text: tab.modelData.label
                verticalAlignment: Text.AlignVCenter
            }

            onClicked: root.selected(tab.modelData.pane)

            // BPM-lock badge: the grid lock stays visible from every pane.
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                anchors.topMargin: 2
                color: tab.activeState ? Theme.textOnAccent : Theme.accent
                height: 2
                radius: 1
                visible: tab.modelData.pane === 2 && root.gridLocked
                width: 12
            }
        }
    }
}
