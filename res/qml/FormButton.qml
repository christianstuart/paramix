import Qt5Compat.GraphicalEffects
import QtQuick 2.12
import QtQuick.Controls 2.12
import "Theme"

/// Segmented/selector button used across the settings pages. It keeps the
/// `backgroundColor` and highlight API of the legacy control but draws a flat,
/// token-driven surface.
AbstractButton {
    id: root

    property color activeColor: Theme.onAccent
    property color backgroundColor: Theme.surface1
    property bool highlight: false
    property color normalColor: Theme.textPrimary
    property color pressedColor: Theme.onAccent

    Accessible.role: Accessible.Button
    Accessible.name: text

    hoverEnabled: true
    implicitHeight: 26
    implicitWidth: 98

    background: Rectangle {
        border.color: root.checked || root.highlight
                ? Theme.accentHover
                : root.hovered
                  ? Theme.borderStrong
                  : Theme.border
        border.width: 1
        color: root.checked || root.highlight
               ? Theme.accent
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
    }
    contentItem: Item {
        anchors.fill: parent

        Label {
            id: label

            anchors.fill: parent
            color: root.checked || root.highlight
                   ? root.activeColor
                   : root.pressed
                     ? root.pressedColor
                     : root.normalColor
            font.bold: true
            font.capitalization: Font.AllUppercase
            font.family: Theme.fontFamily
            font.letterSpacing: Theme.letterSpacingCaps
            font.pixelSize: Theme.buttonFontPixelSize
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
            color: root.checked || root.highlight ? root.activeColor : root.normalColor
            source: image
            visible: root.icon.source != null && root.icon.source != ""
        }
    }
}
