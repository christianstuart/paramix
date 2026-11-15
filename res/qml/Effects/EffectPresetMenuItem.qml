import QtQuick
import QtQuick.Controls
import "../Theme"

/// Menu item used by the effect preset menus. Colours default to the theme
/// tokens; the parent menus can still override them.
MenuItem {
    id: root

    property url checkedSource
    property color disabledTextColor: Theme.textMuted
    property string fontFamily: Theme.fontFamily
    property int fontPixelSize: Theme.fontSizeMd
    property color hoverColor: Qt.alpha(Theme.accent, 0.12)
    property color hoverTextColor: Theme.textPrimary
    property url submenuArrowSource
    property color textColor: Theme.textPrimary

    implicitHeight: visible ? 26 : 0
    implicitWidth: Math.max(160, contentItem.implicitWidth + leftPadding + rightPadding)
    leftPadding: checkable ? 22 : Theme.space2
    rightPadding: subMenu ? 22 : Theme.space2

    arrow: Image {
        anchors.right: parent.right
        anchors.rightMargin: 6
        anchors.verticalCenter: parent.verticalCenter
        fillMode: Image.PreserveAspectFit
        height: 10
        source: root.submenuArrowSource
        visible: root.subMenu && root.enabled
        width: 6
    }
    background: Rectangle {
        color: root.highlighted && root.enabled ? root.hoverColor : "transparent"
        radius: Theme.radiusXs
    }
    contentItem: Text {
        color: !root.enabled ? root.disabledTextColor : root.highlighted ? root.hoverTextColor : root.textColor
        elide: Text.ElideRight
        font.family: root.fontFamily
        font.pixelSize: root.fontPixelSize
        horizontalAlignment: Text.AlignLeft
        text: root.text
        verticalAlignment: Text.AlignVCenter
    }
    indicator: Image {
        anchors.left: parent.left
        anchors.leftMargin: 4
        anchors.verticalCenter: parent.verticalCenter
        fillMode: Image.PreserveAspectFit
        height: 14
        source: root.checked ? root.checkedSource : ""
        visible: root.checkable && root.checked
        width: 14
    }
}
