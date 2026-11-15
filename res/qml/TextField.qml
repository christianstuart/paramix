import QtQuick 2.12
import QtQuick.Controls 2.12
import "Theme"

/// Flat text input. The surface is recessed (surface0) with a single hairline
/// border; focus is the only state that takes the accent ring.
TextField {
    id: root

    color: Theme.textPrimary
    font.family: Theme.fontFamily
    font.pixelSize: Theme.fontSizeMd
    font.weight: Theme.fontWeightRegular
    hoverEnabled: true
    leftPadding: Theme.space3
    rightPadding: Theme.space3
    topPadding: Theme.space1
    bottomPadding: Theme.space1
    placeholderTextColor: Theme.textMuted
    selectByMouse: true
    selectionColor: Theme.accent
    selectedTextColor: Theme.textOnAccent

    background: Rectangle {
        border.color: root.activeFocus
                ? Theme.focusRing
                : root.hovered
                  ? Theme.borderStrong
                  : Theme.border
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
