import QtQuick 2.12
import QtQuick.Controls 2.12
import "Theme"

TextField {
    id: root

    color: Theme.textPrimary
    font.family: Theme.fontFamily
    font.pixelSize: Theme.fontSizeMd
    leftPadding: Theme.space3
    rightPadding: Theme.space3
    topPadding: Theme.space1
    bottomPadding: Theme.space1
    selectByMouse: true
    selectionColor: Theme.accent
    selectedTextColor: Theme.textOnAccent

    background: Rectangle {
        border.color: root.activeFocus ? Theme.focusRing : Theme.border
        border.width: 1
        color: Theme.surface0
        radius: Theme.radiusSm

        Behavior on border.color {
            ColorAnimation {
                duration: Theme.motionFast
            }
        }
    }
}
