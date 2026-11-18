pragma ComponentBehavior: Bound

import "." as Skin
import QtQuick 2.12
import QtQuick.Layouts 1.12
import "Theme"

/// Row of sampler slots, laid out as flat cards with a small gutter.
ColumnLayout {
    id: root

    property int fxUnitCount: 4
    property int hotcueCount: 8
    property bool minimized: false
    property int page: 0
    property int samplerCount: 16
    property bool showFxAssignments: true
    property bool showHotcues: true
    property bool showRateControl: true

    spacing: Theme.space1

    RowLayout {
        Layout.fillWidth: true

        Skin.Button {
            Accessible.name: qsTr("Previous sampler page")
            enabled: root.page > 0
            text: qsTr("Previous")
            onClicked: root.page = Math.max(0, root.page - 1)
        }
        Text {
            Layout.fillWidth: true
            Accessible.name: qsTr("Sampler page %1 of %2").arg(root.page + 1).arg(Math.max(1, Math.ceil(root.samplerCount / 8)))
            color: Theme.textSecondary
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("Page %1 of %2").arg(root.page + 1).arg(Math.max(1, Math.ceil(root.samplerCount / 8)))
        }
        Skin.Button {
            Accessible.name: qsTr("Next sampler page")
            enabled: (root.page + 1) * 8 < root.samplerCount
            text: qsTr("Next")
            onClicked: root.page = Math.min(Math.ceil(root.samplerCount / 8) - 1, root.page + 1)
        }
        Skin.ControlButton {
            Accessible.name: qsTr("Load sampler bank")
            group: "[Sampler]"
            key: "LoadSamplerBank"
            text: qsTr("Load bank")
        }
        Skin.ControlButton {
            Accessible.name: qsTr("Save sampler bank")
            group: "[Sampler]"
            key: "SaveSamplerBank"
            text: qsTr("Save bank")
        }
    }

    GridLayout {
        Layout.fillWidth: true
        columnSpacing: Theme.space1
        columns: 8
        rowSpacing: Theme.space1

        Repeater {
            model: Math.min(8, Math.max(0, root.samplerCount - root.page * 8))

            Skin.Sampler {
                required property int index

                Layout.fillWidth: true
                fxUnitCount: root.fxUnitCount
                group: "[Sampler" + (root.page * 8 + index + 1) + "]"
                hotcueCount: root.hotcueCount
                minimized: root.minimized
                showFxAssignments: root.showFxAssignments
                showHotcues: root.showHotcues
                showRateControl: root.showRateControl
            }
        }
    }
}
