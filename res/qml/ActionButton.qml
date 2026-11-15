import QtQuick
import QtQuick.Controls 2.12
import "Theme"

/// Compact action button used in dialogs and the library context menu.
/// `Category` keeps the semantic color (danger/action) but the shape and
/// typography come from the design tokens. The category tints the label and
/// border only; the surface stays neutral so a row of actions reads calmly.
AbstractButton {
    id: root

    enum Category {
        None,
        Danger,
        Action
    }

    property var category: ActionButton.Category.None
    property alias label: labelField

    readonly property bool danger: root.category === ActionButton.Category.Danger
    readonly property bool action: root.category === ActionButton.Category.Action

    Accessible.role: Accessible.Button
    Accessible.name: labelField.text

    hoverEnabled: true
    implicitHeight: 28

    background: Rectangle {
        border.color: root.danger
                ? Qt.alpha(Theme.danger, 0.65)
                : root.action
                  ? Qt.alpha(Theme.accent, 0.65)
                  : root.hovered
                    ? Theme.borderStrong
                    : Theme.border
        border.width: 1
        color: root.pressed
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
    contentItem: Label {
        id: labelField

        anchors.fill: parent
        color: root.danger
               ? Theme.danger
               : root.action
                 ? Theme.accent
                 : root.hovered
                   ? Theme.textPrimary
                   : Theme.textSecondary
        elide: Text.ElideRight
        font.capitalization: Font.MixedCase
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeSm
        font.weight: Theme.fontWeightMedium
        horizontalAlignment: Text.AlignHCenter
        leftPadding: Theme.space2
        rightPadding: Theme.space2
        verticalAlignment: Text.AlignVCenter
    }
}
