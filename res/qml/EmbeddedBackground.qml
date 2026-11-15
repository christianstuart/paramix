import QtQuick 2.12
import "Theme"

/// Inset surface used for lists, popups and grouped controls. One hairline
/// border, no inner shadow; `radiusSm` keeps compact groups from looking like
/// rounded cards.
Rectangle {
    border.color: Theme.border
    border.width: 1
    color: Theme.surface0
    radius: Theme.radiusSm
}
