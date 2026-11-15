import "." as Skin
import QtQuick 2.12
import "Theme"

/// Microphone and auxiliary units either side of the master crossfader.
Item {
    id: root

    required property real crossfaderWidth

    implicitHeight: crossfader.height

    Skin.SectionBackground {
        anchors.fill: microphoneRow
    }
    Row {
        id: microphoneRow

        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: crossfader.left
        anchors.top: parent.top
        layoutDirection: Qt.RightToLeft
        padding: Theme.space1
        spacing: Theme.space2

        Skin.MicrophoneUnit {
            unitNumber: 1
        }
        Skin.MicrophoneUnit {
            unitNumber: 2
        }
        Skin.MicrophoneUnit {
            unitNumber: 3
        }
        Skin.MicrophoneUnit {
            unitNumber: 4
        }
        Skin.MicrophoneDuckingPanel {
        }
    }
    Skin.SectionBackground {
        id: crossfader

        anchors.centerIn: parent
        height: crossfaderSlider.height + 2 * Theme.space2
        width: root.crossfaderWidth

        Skin.ControlFader {
            id: crossfaderSlider

            Accessible.name: qsTr("Crossfader")
            Accessible.role: Accessible.Slider

            anchors.left: parent.left
            anchors.leftMargin: Theme.space2
            anchors.right: parent.right
            anchors.rightMargin: Theme.space2
            anchors.verticalCenter: parent.verticalCenter
            bar.color: Theme.accent
            bar.start: 0.5
            bg: Theme.imgCrossfaderBackground
            fg: Theme.imgCrossfaderHandle
            group: "[Master]"
            key: "crossfader"
            orientation: Qt.Horizontal

            handleImage {
                height: 28
            }
        }
    }
    Skin.SectionBackground {
        anchors.bottom: parent.bottom
        anchors.left: crossfader.right
        anchors.right: parent.right
        anchors.top: parent.top
    }
    Row {
        id: auxiliaryRow

        anchors.bottom: parent.bottom
        anchors.left: crossfader.right
        anchors.right: parent.right
        anchors.top: parent.top
        padding: Theme.space1
        spacing: Theme.space2

        Skin.AuxiliaryUnit {
            layoutDirection: Qt.RightToLeft
            unitNumber: 1
        }
        Skin.AuxiliaryUnit {
            layoutDirection: Qt.RightToLeft
            unitNumber: 2
        }
        Skin.AuxiliaryUnit {
            layoutDirection: Qt.RightToLeft
            unitNumber: 3
        }
        Skin.AuxiliaryUnit {
            layoutDirection: Qt.RightToLeft
            unitNumber: 4
        }
    }
}
