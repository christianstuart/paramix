import "." as Skin
import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Shapes
import "Theme"

ComboBox {
    id: root

    property bool clip: false
    property list<var> footerItems: []
    property int popupMaxItem: 6
    property alias popupWidth: popupItem.width
    /// Left inset for the closed-combo text. Effects selectors use it to clear
    /// the dropdown chevron, which the flat style draws at the left edge.
    property int textLeftPadding: Theme.space1

    readonly property int itemHeight: 28
    readonly property real windowHeight: Window.height

    signal activateFooter(int index)

    font.family: Theme.fontFamily
    font.pixelSize: Theme.fontSizeSm
    font.weight: Theme.fontWeightRegular
    hoverEnabled: true
    implicitHeight: 26
    leftPadding: Theme.space2
    rightPadding: Theme.space2

    background: Rectangle {
        border.color: root.activeFocus
                ? Theme.focusRing
                : root.hovered || root.popup.visible
                  ? Theme.borderStrong
                  : Theme.border
        border.width: 1
        color: root.hovered || root.popup.visible ? Theme.surface2 : Theme.surface1
        radius: Theme.radiusSm

        Behavior on color {
            ColorAnimation {
                duration: Theme.motionFast
            }
        }
        Behavior on border.color {
            ColorAnimation {
                duration: Theme.motionFast
            }
        }
    }
    contentItem: Text {
        rightPadding: root.indicator.width + Theme.space1
        clip: root.clip
        color: Theme.textPrimary
        elide: root.clip ? Text.ElideNone : Text.ElideRight
        font: root.font
        leftPadding: root.textLeftPadding
        text: root.displayText
        verticalAlignment: Text.AlignVCenter
    }
    indicator: Item {
        height: root.height
        width: 18

        Shape {
            anchors.centerIn: parent
            height: 6
            visible: parent.width > 0
            width: 10

            ShapePath {
                fillColor: "transparent"
                startX: 1
                startY: 1
                strokeColor: root.popup.visible ? Theme.textPrimary : Theme.textSecondary
                strokeWidth: 1.4

                PathLine {
                    x: 5
                    y: 5
                }
                PathLine {
                    x: 9
                    y: 1
                }
            }
        }
    }
    delegate: ItemDelegate {
        id: itemDlgt

        required property int index

        highlighted: root.highlightedIndex === itemDlgt.index
        implicitHeight: root.itemHeight
        padding: 0
        text: root.textAt(itemDlgt.index)
        // Popup rows fill the list; using the trigger width clipped every name.
        width: ListView.view.width

        background: Rectangle {
            color: itemDlgt.highlighted
                   ? Qt.alpha(Theme.accent, 0.16)
                   : itemDlgt.hovered
                     ? Theme.surface3
                     : "transparent"
            radius: Theme.radiusXs
        }
        contentItem: Text {
            color: itemDlgt.highlighted ? Theme.accent : Theme.textPrimary
            elide: Text.ElideRight
            font: root.font
            leftPadding: Theme.space2
            rightPadding: Theme.space2
            text: itemDlgt.text
            verticalAlignment: Text.AlignVCenter
        }
    }
    popup: Popup {
        id: popupItem

        height: root.itemHeight * (Math.min(root.popupMaxItem, Math.max(root.count, 1)) + 1)
        padding: 0
        width: root.width
        // Right-aligned, but never past the left window edge (a narrow trigger
        // such as the mixer's 36 px QuickFX selector opens a 100 px list).
        x: Math.max(root.width - width, -root.mapToItem(null, 0, 0).x)
        // Open upward when the list would fall off the bottom of the window;
        // the mixer's dropdowns sit low on the screen.
        y: (root.mapToItem(null, 0, 0).y + root.height + height > root.windowHeight
            && root.mapToItem(null, 0, 0).y - height > 0) ? -height + 2 : root.height - 2

        background: Rectangle {
            border.color: Theme.borderStrong
            border.width: 1
            color: Theme.surface2
            radius: Theme.radiusSm
        }
        contentItem: ListView {
            id: listView

            clip: true
            currentIndex: root.highlightedIndex
            model: root.popup.visible ? root.delegateModel : null
            spacing: 1

            ScrollIndicator.vertical: ScrollIndicator {
            }
            footer: Item {
                anchors.left: parent.left
                anchors.right: parent.right
                height: childrenRect.height

                Repeater {
                    model: root.footerItems

                    Rectangle {
                        y: index * height
                        anchors.left: parent.left
                        anchors.right: parent.right
                        color: "transparent"
                        height: root.itemHeight

                        Item {
                            anchors.fill: parent
                            anchors.margins: Theme.space1

                            Text {
                                anchors.bottom: parent.bottom
                                anchors.top: parent.top
                                color: Theme.textPrimary
                                elide: Text.ElideRight
                                font: root.font
                                text: modelData.text
                                verticalAlignment: Text.AlignVCenter
                            }
                            Text {
                                anchors.bottom: parent.bottom
                                anchors.right: parent.right
                                anchors.top: parent.top
                                color: Theme.textMuted
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeSm
                                text: modelData?.suffix || ''
                            }
                        }
                        MouseArea {
                            id: footerItemMouseArea

                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor

                            onPressed: {
                                root.activateFooter(index);
                            }
                        }
                    }
                }
            }
        }
    }
}
