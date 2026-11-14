import QtQuick
import QtQuick.Controls
import "../Theme"

/// Hairline separator for the effect preset menus.
MenuSeparator {
    id: root

    property color lineColor: Theme.border

    implicitHeight: 9

    contentItem: Rectangle {
        color: root.lineColor
        height: 1
        width: parent.width - 2 * Theme.space1
        x: Theme.space1
        y: 4
    }
}
