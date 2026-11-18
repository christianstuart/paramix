pragma ComponentBehavior: Bound

import "." as Skin
import Mixxx 1.0 as Mixxx
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
    readonly property int pageCount: Math.max(1, Math.ceil(root.samplerCount / 8))
    property int samplerCount: 16
    property bool showFxAssignments: true
    property bool showHotcues: true
    property bool showRateControl: true

    spacing: Theme.space1

    // shortcut: pager adds 20 px; if 1280x720 with samplers and library on exceeds the library's 120 px floor, raise the rack or hide the waveform.
    RowLayout {
        height: 20
        Layout.preferredHeight: 20
        Layout.fillWidth: true
        spacing: Theme.space1

        Skin.Button {
            Accessible.name: qsTr("Previous sampler page")
            enabled: root.page > 0
            implicitHeight: 20
            text: qsTr("Previous")
            onClicked: root.page = Math.max(0, root.page - 1)
        }
        Text {
            Layout.alignment: Qt.AlignVCenter
            Layout.fillWidth: true
            Accessible.name: qsTr("Sampler page %1 of %2").arg(root.page + 1).arg(root.pageCount)
            color: Theme.textSecondary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeXs
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("Page %1 of %2").arg(root.page + 1).arg(root.pageCount)
        }
        Skin.Button {
            Accessible.name: qsTr("Next sampler page")
            enabled: (root.page + 1) * 8 < root.samplerCount
            implicitHeight: 20
            text: qsTr("Next")
            onClicked: root.page = Math.min(root.pageCount - 1, root.page + 1)
        }
        Skin.Button {
            Accessible.name: qsTr("Load sampler bank")
            implicitHeight: 20
            text: qsTr("Load bank")
            onClicked: {
                loadBankControl.value = 1;
                loadBankControl.value = 0;
            }
        }
        Skin.Button {
            Accessible.name: qsTr("Save sampler bank")
            implicitHeight: 20
            text: qsTr("Save bank")
            onClicked: {
                saveBankControl.value = 1;
                saveBankControl.value = 0;
            }
        }
    }

    onSamplerCountChanged: page = Math.min(page, pageCount - 1)

    GridLayout {
        Layout.fillWidth: true
        columnSpacing: Theme.space1
        columns: Math.min(8, Math.max(1, root.samplerCount - root.page * 8))
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

    Mixxx.ControlProxy {
        id: loadBankControl

        group: "[Sampler]"
        key: "LoadSamplerBank"
    }
    Mixxx.ControlProxy {
        id: saveBankControl

        group: "[Sampler]"
        key: "SaveSamplerBank"
    }
}
