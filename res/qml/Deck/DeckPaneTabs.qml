pragma ComponentBehavior: Bound

import ".." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import QtQuick.Controls 2.15
import QtQuick.Layouts
import "../Theme"

/// Horizontal CUE / MOVE / GRID / PAD FX selector above the deck pane.
///
/// The row is one tab high and spans the pane width, so the deck's transport
/// band stays the same height while the old 34 px vertical column is gone. The
/// active pane is filled by the accent; the grid lock badge stays on the GRID
/// tab so the state is visible from every pane.
RowLayout {
    id: root

    /// 0 = Move (beatjump), 1 = Cue (hotcues), 2 = Grid (beatgrid),
    /// 4 = Pad FX. 3 is left unused: it belonged to a removed stem pane.
    property int activePane: 1
    /// Lights a small badge on the GRID tab while the BPM is locked.
    property bool gridLocked: false
    /// Deck group, used to read and write the per-deck Pad FX bank.
    property string group: ""

    signal selected(int pane)

    readonly property string padFxGroup: root.group.length > 0
            ? root.group.substr(0, root.group.length - 1) + "_PadFX]"
            : ""
    readonly property int padFxBank: Math.max(1, Math.min(2,
            Math.round(padFxBankControl.value)))

    function selectPane(pane, shift) {
        if (pane === 4) {
            padFxBankControl.value = shift ? 2 : 1;
        }
        root.selected(pane);
    }

    objectName: "deckPaneTabs"
    spacing: Theme.space1

    Mixxx.ControlProxy {
        id: padFxBankControl

        group: root.padFxGroup
        key: "bank"
    }

    Repeater {
        // Display order: Cue first (the default pane), then Move, Grid, Pad FX.
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
                "label": qsTr("Pad FX"),
                "pane": 4,
                "hint": qsTr("Show Pad FX pads")
            }
        ]

        delegate: Skin.Button {
            id: tab

            required property var modelData
            readonly property bool current: root.activePane === modelData.pane
            readonly property bool padFxTab: modelData.pane === 4
            readonly property string displayLabel: tab.padFxTab
                    ? qsTr("FX %1").arg(root.padFxBank)
                    : modelData.label
            readonly property string tabHint: tab.padFxTab
                    ? qsTr("Pad FX. Click for FX 1, shift-click for FX 2.")
                    : modelData.hint

            Layout.fillHeight: true
            Layout.fillWidth: true
            // Pin the whole height contract, not just the minimum: the row is
            // 16 px while a plain Button implies 26, and a parent layout that
            // is only told about the cap can shrink the row (and the pane
            // below it) to resolve the deficit. Matching implicit and preferred
            // to the tab height makes the row's own hint 16 as well.
            Layout.maximumHeight: Theme.deckPaneTabHeight
            Layout.minimumHeight: Theme.deckPaneTabHeight
            Layout.preferredHeight: Theme.deckPaneTabHeight
            highlight: tab.current
            implicitHeight: Theme.deckPaneTabHeight
            normalColor: Theme.textMuted

            Accessible.checkable: true
            Accessible.checked: tab.current
            Accessible.description: tab.tabHint
            Accessible.name: qsTr("Show %1 pane").arg(tab.displayLabel)
            Accessible.role: Accessible.RadioButton

            ToolTip.delay: 500
            ToolTip.text: tab.tabHint
            ToolTip.visible: tabHover.hovered

            contentItem: Text {
                color: tab.activeState ? tab.activeColor : tab.hovered ? Theme.textPrimary : Theme.textMuted
                font.capitalization: Font.AllUppercase
                font.family: Theme.fontFamily
                font.letterSpacing: Theme.letterSpacingCaps
                font.pixelSize: Math.min(Theme.fontSizeXs, Theme.deckPaneTabHeight - 4)
                font.weight: Theme.fontWeightDemiBold
                horizontalAlignment: Text.AlignHCenter
                text: tab.displayLabel
                verticalAlignment: Text.AlignVCenter
            }

            // Keyboard activation always selects FX 1. Mouse activation is
            // handled below so it can see the Shift modifier.
            onClicked: root.selectPane(tab.modelData.pane, false)

            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton

                onClicked: mouse => root.selectPane(
                        tab.modelData.pane,
                        (mouse.modifiers & Qt.ShiftModifier) !== 0)
            }
            HoverHandler {
                id: tabHover
            }

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
