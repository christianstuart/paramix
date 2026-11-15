import QtQuick 2.12
import "Theme"

/// Flat panel surface used behind decks, the mixer and the library.
/// Traktor-style matte plate: neutral fill, 1 px hairline frame, no gloss.
Rectangle {
    color: Theme.surface1
    radius: Theme.radiusSm
    border.width: 1
    border.color: Theme.border
}
