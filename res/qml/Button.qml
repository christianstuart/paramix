import Qt5Compat.GraphicalEffects
import QtQuick 2
import QtQuick.Controls 2
import "Theme"

/// Flat, token-driven button. The legacy `activeColor`, `normalColor`,
/// `pressedColor` and `highlight` properties are preserved so existing call
/// sites keep their intent while the visuals come from the design system.
AbstractButton {
    id: root

    property color activeColor: Theme.onAccent
    property bool highlight: false
    property color normalColor: Theme.textSecondary
    property color pressedColor: Theme.textPrimary

    Accessible.role: Accessible.Button
    Accessible.name: text

    hoverEnabled: true
    implicitHeight: 28
    implicitWidth: 64

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
                   : Theme.surface1
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
        anchors.fill: parent

        Text {
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
