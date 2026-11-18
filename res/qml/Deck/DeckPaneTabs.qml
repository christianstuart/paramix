import ".." as Skin
import QtQuick 2.12
import QtQuick.Layouts
import "../Theme"

/// Vertical MOVE / CUE / GRID / STEM / PAD selector for the deck pane stack.
///
/// Traktor puts this stacked selector at the right edge of the loop row, one
/// block wide, with the active pane filled by the accent. The stack is exactly
/// as tall as the loop card next to it, so the two read as one row and neither
/// can overlap the other. The host is a constant height, so switching panes
/// never reflows the transport and the pane strip below stays put.
ColumnLayout {
    id: root

    /// 0 = Move (beatjump), 1 = Cue (hotcues), 2 = Grid (beatgrid),
    /// 3 = Stems (stem mixer lanes), 4 = Pad (per-stem Pad FX banks).
    property int activePane: 1
    /// Lights a small badge on the GRID tab while the BPM is locked.
    property bool gridLocked: false

    signal selected(int pane)

    spacing: Theme.space1

    Repeater {
        // Display order: Cue first (the default pane), then Move, Grid, Stem.
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
            },
            {
                "label": qsTr("Stem"),
                "pane": 3,
                "hint": qsTr("Show the stem mixer")
            },
            {
                "label": qsTr("Pad"),
                "pane": 4,
                "hint": qsTr("Show the per-stem Pad FX banks")
            }
        ]

        delegate: Skin.Button {
            id: tab

            required property var modelData
            readonly property bool current: root.activePane === modelData.pane

            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.minimumHeight: 16
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
                font.pixelSize: Theme.fontSizeXs
                font.weight: Theme.fontWeightDemiBold
                horizontalAlignment: Text.AlignHCenter
                text: tab.modelData.label
                verticalAlignment: Text.AlignVCenter
            }

            onClicked: root.selected(tab.modelData.pane)

            // BPM-lock badge: the grid lock stays visible from every pane, so
            // dropping the toolbar's Beatgrid key does not hide that state.
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                anchors.topMargin: 3
                color: tab.activeState ? Theme.textOnAccent : Theme.accent
                height: 3
                radius: 1.5
                visible: tab.modelData.pane === 2 && root.gridLocked
                width: 12
            }
        }
    }
}
