import Qt5Compat.GraphicalEffects
import QtQuick 2.12
import QtQuick.Controls 2.12
import "Theme"

/// Segmented/selector button used across the settings pages. It keeps the
/// `backgroundColor` and highlight API of the legacy control but draws a flat,
/// token-driven surface. Idle buttons are neutral; only the selected segment
/// takes the accent fill.
AbstractButton {
    id: root

    property color activeColor: Theme.textOnAccent
    property color backgroundColor: Theme.surface1
    property bool highlight: false
    property color normalColor: Theme.textPrimary
    property color pressedColor: Theme.textOnAccent

    readonly property bool activeState: root.checked || root.highlight

    Accessible.role: Accessible.Button
    Accessible.name: root.text

    hoverEnabled: true
    implicitHeight: 26
    implicitWidth: 98

    background: Rectangle {
        border.color: root.activeState
                ? Theme.accent
                : root.activeFocus
                  ? Theme.focusRing
                  : root.hovered
                    ? Theme.borderStrong
                    : Theme.border
        border.width: 1
        color: root.activeState
               ? root.pressed
                 ? Theme.accentPressed
                 : Theme.accent
               : root.pressed
                 ? Theme.surface3
                 : root.hovered
                   ? Theme.surface2
                   : root.backgroundColor
        radius: Theme.radiusSm

        Behavior on color {
            ColorAnimation {
                duration: Theme.motionFast
            }
        }
        Behavior on border.color {
            ColorAnimation {
                duration: Theme.motionFast
            }
        }
    }
    contentItem: Item {
        Label {
            id: label

            anchors.fill: parent
            color: root.activeState
                   ? root.activeColor
                   : root.pressed
                     ? root.pressedColor
                     : root.normalColor
            elide: Text.ElideRight
            font.capitalization: Font.MixedCase
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeSm
            font.weight: Theme.fontWeightMedium
            horizontalAlignment: Text.AlignHCenter
            text: root.text
            verticalAlignment: Text.AlignVCenter
            visible: root.text != null && root.text.length > 0
        }
        Image {
            id: image

            anchors.centerIn: parent
            asynchronous: true
            fillMode: Image.PreserveAspectFit
            height: root.icon.height
            source: root.icon.source
            visible: false
            width: root.icon.width
        }
        ColorOverlay {
            anchors.fill: image
            antialiasing: true
            color: root.activeState ? root.activeColor : root.normalColor
            source: image
            visible: root.icon.source != null && root.icon.source != ""
        }
    }
}
