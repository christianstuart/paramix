import QtQuick 2.12
import "Theme"

/// Section heading used in panels and the settings pages.
Text {
    color: Theme.textSecondary
    elide: Text.ElideRight
    font.bold: true
    font.capitalization: Font.AllUppercase
    font.family: Theme.fontFamily
    font.letterSpacing: Theme.letterSpacingCaps
    font.pixelSize: Theme.fontSizeXs
    horizontalAlignment: Text.AlignHCenter
    verticalAlignment: Text.AlignVCenter
}
