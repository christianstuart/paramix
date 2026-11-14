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

    signal activateFooter(int index)

    font.family: Theme.fontFamily
    font.pixelSize: Theme.fontSizeSm
    hoverEnabled: true
    indicator.width: 18

    background: Rectangle {
        border.color: root.activeFocus
                ? Theme.focusRing
                : root.hovered
                  ? Theme.borderStrong
                  : Theme.border
        border.width: 1
        color: root.hovered ? Theme.surface2 : Theme.surface1
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
        clip: root.clip
        color: Theme.textPrimary
        elide: root.clip ? Text.ElideNone : Text.ElideRight
        font: root.font
        leftPadding: Theme.space2
        text: root.displayText
        verticalAlignment: Text.AlignVCenter
    }
    delegate: ItemDelegate {
        id: itemDlgt

        required property int index

        highlighted: root.highlightedIndex === this.index
        padding: Theme.space2
        text: root.textAt(this.index)
        width: root.width

        background: Rectangle {
            color: itemDlgt.highlighted
                   ? Qt.alpha(Theme.accent, 0.22)
                   : itemDlgt.hovered
                     ? Theme.surface3
                     : "transparent"
            radius: Theme.radiusXs
        }
        contentItem: Text {
            color: itemDlgt.highlighted ? Theme.accent : Theme.textPrimary
            elide: Text.ElideRight
            font: root.font
            text: itemDlgt.text
            verticalAlignment: Text.AlignVCenter
        }
    }
    popup: Popup {
        id: popupItem

        height: root.contentItem.height * (Math.min(root.popupMaxItem, Math.max(root.count, 1)) + 1)
        padding: 0
        width: root.width
        x: root.width - width
        y: root.height - 4

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
                        height: root.contentItem.height

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
                                color: Theme.textSecondary
                                font.pixelSize: Theme.fontSizeMd
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
