import QtQuick 2.12
import "Theme"

/// Small uppercase eyebrow used for section headings and strip labels. This is
/// the one place where all-caps + tracking is allowed; body text and buttons
/// stay mixed case.
Text {
    color: Theme.textSecondary
    elide: Text.ElideRight
    font.capitalization: Font.AllUppercase
    font.family: Theme.fontFamily
    font.letterSpacing: Theme.letterSpacingCaps
    font.pixelSize: Theme.fontSizeXs
    font.weight: Theme.fontWeightDemiBold
    horizontalAlignment: Text.AlignHCenter
    verticalAlignment: Text.AlignVCenter
}
