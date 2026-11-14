import QtQuick
import QtQuick.Controls 2.12
import "Theme"

/// Compact action button used in dialogs and the library context menu.
/// `Category` keeps the semantic color (danger/action) but the shape and
/// typography come from the design tokens.
AbstractButton {
    id: root

    enum Category {
        None,
        Danger,
        Action
    }

    property var category: ActionButton.Category.None
    property alias label: labelField

    Accessible.role: Accessible.Button
    Accessible.name: labelField.text

    hoverEnabled: true
    implicitHeight: 28

    background: Rectangle {
        border.color: root.category === ActionButton.Category.Danger
                ? Theme.danger
                : root.category === ActionButton.Category.Action
                  ? Theme.accentHover
                  : root.hovered
                    ? Theme.borderStrong
                    : Theme.border
        border.width: 1
        color: root.pressed
               ? Theme.surface3
               : root.hovered
                 ? Theme.surface2
                 : root.category === ActionButton.Category.Danger
                   ? Qt.alpha(Theme.danger, 0.18)
                   : root.category === ActionButton.Category.Action
                     ? Qt.alpha(Theme.accent, 0.22)
                     : Theme.surface1
        radius: Theme.radiusSm

        Behavior on color {
            ColorAnimation {
                duration: Theme.motionFast
            }
        }
    }
    contentItem: Item {
        Label {
            id: labelField

            anchors.fill: parent
            color: root.category === ActionButton.Category.Danger
                   ? Theme.danger
                   : root.category === ActionButton.Category.Action
                     ? Theme.accent
                     : Theme.textPrimary
            font.bold: true
            font.capitalization: Font.AllUppercase
            font.family: Theme.fontFamily
            font.letterSpacing: Theme.letterSpacingCaps
            font.pixelSize: Theme.fontSizeXs
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }
}
