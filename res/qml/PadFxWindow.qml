pragma ComponentBehavior: Bound

import "." as Skin
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts
import QtQuick.Window 2.15
import "Theme"

/// Dedicated Pad FX workspace. Closed by default; opened from the toolbar or
/// the View menu. Four deck columns, each with its own A-D bank.
Window {
    id: root

    color: Theme.surface0
    height: 720
    minimumHeight: 480
    minimumWidth: 760
    title: qsTr("Pad FX")
    visible: false
    width: 1240

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.space2
        spacing: Theme.space2

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space2

            Text {
                Layout.fillWidth: true
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeLg
                font.weight: Theme.fontWeightDemiBold
                text: qsTr("Pad FX")
            }
            Text {
                color: Theme.textMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeSm
                text: qsTr("Per-stem performance effects")
            }
            Skin.Button {
                implicitHeight: 26
                text: qsTr("Close")

                onClicked: root.close()
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.space2

            Repeater {
                model: 4

                delegate: Skin.PadFxDeckPanel {
                    required property int index

                    Layout.fillHeight: true
                    Layout.fillWidth: true
                    group: "[Channel" + (index + 1) + "]"
                }
            }
        }
    }
}
