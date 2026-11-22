import ".." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Shapes
import "../Theme"

/// Effect selector for one slot. The list is split into Beat and Color tabs,
/// which the single-list skin combo box cannot host, so the closed face and the
/// popup are built here. The public interface stays the same as the combo box
/// it replaces: `slot`, `font`, `implicitHeight`, `Accessible.name` and the
/// Layout attached properties its callers set.
Skin.Button {
    id: root

    required property Mixxx.EffectSlotProxy slot

    readonly property bool beatFxLoaded: root.slot?.isBeatFx ?? false
    readonly property string displayName: (root.slot?.loaded ?? false) ? root.slot.effectName : ""
    readonly property string observedEffectId: root.slot?.effectId ?? ""

    function effectIdAt(model, index) {
        // The C++ model has no `count`; an out-of-range row has no effectId.
        if (!model || index < 0) {
            return "";
        }
        return model.get(index).effectId || "";
    }
    function choose(index) {
        if (index < 0) {
            return;
        }
        const effectId = root.effectIdAt(popup.currentModel, index);
        if (root.slot.effectId !== effectId) {
            root.slot.effectId = effectId;
        }
        popup.close();
    }
    /// Switch the popup to one list and mark the loaded effect in it.
    function showTab(beat) {
        popup.beatTab = beat;
        root.selectLoadedEffect();
        // A clicked tab key takes focus; hand it back so Return still chooses
        // and the arrows keep paging the list.
        popupContent.forceActiveFocus();
    }
    function selectLoadedEffect() {
        const model = popup.currentModel;
        let index = 0;
        for (let row = 0; row < effectList.count; ++row) {
            if (root.effectIdAt(model, row) === root.observedEffectId) {
                index = row;
                break;
            }
        }
        effectList.currentIndex = index;
        effectList.positionViewAtIndex(index, ListView.Contain);
    }

    Accessible.name: qsTr("Effect")
    Accessible.role: Accessible.ComboBox
    ToolTip.delay: 500
    ToolTip.text: root.displayName
    ToolTip.visible: root.hovered && root.displayName.length > 0
    font.family: Theme.fontFamily
    implicitHeight: Theme.fxControlHeight
    implicitWidth: Theme.fxSlotMinWidth

    onClicked: popup.opened ? popup.close() : popup.open()

    contentItem: Item {
        Text {
            id: beatTag

            anchors.left: parent.left
            anchors.leftMargin: Theme.space1
            anchors.verticalCenter: parent.verticalCenter
            color: Theme.accent
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeXs
            font.weight: Theme.fontWeightDemiBold
            text: qsTr("BEAT")
            visible: root.beatFxLoaded
        }
        Text {
            anchors.left: beatTag.visible ? beatTag.right : parent.left
            anchors.leftMargin: Theme.space1
            anchors.right: chevron.left
            anchors.rightMargin: Theme.space1
            anchors.verticalCenter: parent.verticalCenter
            color: Theme.textPrimary
            elide: Text.ElideRight
            font: root.font
            text: root.displayName
            verticalAlignment: Text.AlignVCenter
        }
        Shape {
            id: chevron

            anchors.right: parent.right
            anchors.rightMargin: Theme.space1
            anchors.verticalCenter: parent.verticalCenter
            height: Theme.space1 + Theme.radiusSm
            width: Theme.fontSizeXs

            ShapePath {
                fillColor: "transparent"
                startX: 1
                startY: 1
                strokeColor: popup.visible ? Theme.textPrimary : Theme.textSecondary
                strokeWidth: 1

                PathLine {
                    x: chevron.width / 2
                    y: chevron.height - 1
                }
                PathLine {
                    x: chevron.width - 1
                    y: 1
                }
            }
        }
    }
    Popup {
        id: popup

        readonly property var currentModel: popup.beatTab ? Mixxx.EffectsManager.beatEffectsModel : Mixxx.EffectsManager.colorEffectsModel
        // ListView.count, because the C++ model exposes no `count` property.
        readonly property int rowCount: Math.min(8, Math.max(1, effectList.count))
        property bool beatTab: true

        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        focus: true
        padding: 0
        parent: root
        width: Math.max(root.width, Theme.fxEffectPopupMinWidth)
        x: Math.max(root.width - popup.width, -root.mapToItem(null, 0, 0).x)
        y: {
            const win = root.Window.window;
            const top = root.mapToItem(null, 0, 0).y;
            if (win && top + root.height + popup.height > win.height && top - popup.height > 0) {
                return -popup.height + Theme.radiusSm;
            }
            return root.height - Theme.radiusSm;
        }

        onOpened: {
            // An empty slot opens on Beat, a loaded one on the loaded kind.
            root.showTab(root.beatFxLoaded || !root.slot.loaded);
        }

        contentItem: ColumnLayout {
            id: popupContent

            focus: true
            spacing: Theme.space1
            width: popup.availableWidth

            Keys.onPressed: event => {
                if (event.key === Qt.Key_Down) {
                    effectList.currentIndex = Math.min(effectList.count - 1, effectList.currentIndex + 1);
                } else if (event.key === Qt.Key_Up) {
                    effectList.currentIndex = Math.max(0, effectList.currentIndex - 1);
                } else if (event.key === Qt.Key_Left) {
                    root.showTab(true);
                } else if (event.key === Qt.Key_Right) {
                    root.showTab(false);
                } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                    root.choose(effectList.currentIndex);
                } else {
                    return;
                }
                event.accepted = true;
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: Theme.space1
                Layout.rightMargin: Theme.space1
                Layout.topMargin: Theme.space1
                spacing: Theme.space1

                Skin.Button {
                    Accessible.name: qsTr("Beat effects")
                    Layout.fillWidth: true
                    ToolTip.delay: 500
                    ToolTip.text: Accessible.name
                    ToolTip.visible: hovered
                    highlight: popup.beatTab
                    implicitHeight: Theme.fxControlHeight
                    text: qsTr("Beat")

                    onClicked: root.showTab(true)
                }
                Skin.Button {
                    Accessible.name: qsTr("Color effects")
                    Layout.fillWidth: true
                    ToolTip.delay: 500
                    ToolTip.text: Accessible.name
                    ToolTip.visible: hovered
                    highlight: !popup.beatTab
                    implicitHeight: Theme.fxControlHeight
                    text: qsTr("Color")

                    onClicked: root.showTab(false)
                }
            }
            ListView {
                id: effectList

                Layout.bottomMargin: Theme.space1
                Layout.fillWidth: true
                Layout.leftMargin: Theme.space1
                Layout.preferredHeight: popup.rowCount * Theme.fxEffectRowHeight
                Layout.rightMargin: Theme.space1
                clip: true
                keyNavigationEnabled: false
                model: popup.currentModel
                spacing: 1

                delegate: Rectangle {
                    id: effectRow

                    required property string display
                    required property int index
                    required property string tooltip

                    readonly property bool isCurrentEffect: root.effectIdAt(popup.currentModel, effectRow.index) === root.observedEffectId

                    Accessible.name: effectRow.display
                    Accessible.role: Accessible.Button
                    color: ListView.isCurrentItem ? Theme.accentSoft : rowHover.hovered ? Theme.surface3 : "transparent"
                    height: Theme.fxEffectRowHeight
                    radius: Theme.radiusXs
                    width: ListView.view.width

                    ToolTip.delay: 500
                    ToolTip.text: effectRow.tooltip
                    ToolTip.visible: rowHover.hovered && effectRow.tooltip.length > 0

                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: Theme.space2
                        anchors.right: checkMark.left
                        anchors.rightMargin: Theme.space1
                        anchors.verticalCenter: parent.verticalCenter
                        color: effectRow.isCurrentEffect ? Theme.accent : Theme.textPrimary
                        elide: Text.ElideRight
                        font: root.font
                        text: effectRow.display
                    }
                    Text {
                        id: checkMark

                        anchors.right: parent.right
                        anchors.rightMargin: Theme.space2
                        anchors.verticalCenter: parent.verticalCenter
                        color: Theme.accent
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeXs
                        text: "\u2713"
                        visible: effectRow.isCurrentEffect
                    }
                    HoverHandler {
                        id: rowHover

                        cursorShape: Qt.PointingHandCursor
                    }
                    TapHandler {
                        onTapped: root.choose(effectRow.index)
                    }
                }
            }
        }
    }
}
