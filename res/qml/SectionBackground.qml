import QtQuick 2.12
import "Theme"

/// Flat panel surface used behind decks, the mixer and the library.
/// Replaces the legacy nine-slice bitmap so the skin can follow the theme
/// tokens (surface, border, radius) instead of a fixed image.
Rectangle {
    color: Theme.surface1
    radius: Theme.radiusMd
    border.width: 1
    border.color: Theme.border
}
