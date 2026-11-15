import QtQuick
import QtQuick.Controls
import "Theme"

/// Checkable menu entry used by the compact toolbar dropdowns. The checked
/// state is a filled accent square so it reads at a glance without an icon
/// asset; the label dims when the entry is disabled.
MenuItem {
    id: root

    implicitHeight: visible ? 28 : 0
    implicitWidth: Math.max(150, contentItem.implicitWidth + leftPadding + rightPadding)
    leftPadding: 24
    rightPadding: Theme.space3

    background: Rectangle {
        color: root.highlighted || root.pressed ? Qt.alpha(Theme.accent, 0.12) : "transparent"
        radius: Theme.radiusXs
    }
    contentItem: Text {
        color: !root.enabled ? Theme.textMuted : root.highlighted ? Theme.textPrimary : Theme.textSecondary
        elide: Text.ElideRight
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeSm
        text: root.text
        verticalAlignment: Text.AlignVCenter
    }
    indicator: Rectangle {
        anchors.left: parent.left
        anchors.leftMargin: Theme.space2
        anchors.verticalCenter: parent.verticalCenter
        border.color: root.checked ? Theme.accent : Theme.borderStrong
        border.width: 1
        color: root.checked ? Theme.accent : "transparent"
        height: 9
        opacity: root.enabled ? 1.0 : 0.5
        radius: Theme.radiusXs
        width: 9
    }
}
