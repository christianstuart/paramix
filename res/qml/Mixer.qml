import "." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import QtQuick.Layouts
import "Theme"

/// Channel mixer for two or four decks.
///
/// The mixer keeps a single flat surface. The left and right channel pairs are
/// separated by one hairline so the strips read as two banks, and the deck
/// assignment buttons for the crossfader live in a recessed strip at the
/// bottom.
Item {
    id: root

    required property var groups
    property bool show4decks: false

    readonly property int stripWidth: 42

    implicitHeight: content.height + crossfader.height
    implicitWidth: 190

    Skin.SectionBackground {
        anchors.fill: parent
    }
    Column {
        anchors.fill: parent

        Item {
            id: content

            height: (root.show4decks ? eqDeck1.height * 2 : eqDeck1.height) + 2 * Theme.space1
            width: root.implicitWidth

            Item {
                id: stripArea

                anchors.fill: parent
                anchors.margins: Theme.space1

                states: [
                    State {
                        name: "2decks"
                        when: root.show4decks

                        AnchorChanges {
                            target: eqDeck1

                            anchors {
                                left: eqDeck3.right
                                top: parent.top
                            }
                        }
                        AnchorChanges {
                            target: mixerDeck1

                            anchors {
                                left: mixerDeck3.right
                                top: eqDeck1.bottom
                            }
                        }
                        AnchorChanges {
                            target: mixerDeck2

                            anchors {
                                right: mixerDeck4.left
                                top: eqDeck2.bottom
                            }
                        }
                        AnchorChanges {
                            target: eqDeck2

                            anchors {
                                right: eqDeck4.left
                                top: parent.top
                            }
                        }
                    }
                ]

                // Hairline between the left and the right channel bank.
                Rectangle {
                    anchors.bottom: parent.bottom
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    color: Theme.border
                    width: 1
                }

                Skin.EqColumn {
                    id: eqDeck3

                    group: root.groups[2]
                    visible: root.show4decks
                    width: root.stripWidth

                    anchors {
                        left: parent.left
                        top: parent.top
                    }
                }
                Skin.EqColumn {
                    id: eqDeck1

                    group: root.groups[0]
                    width: root.stripWidth

                    anchors {
                        left: parent.left
                        leftMargin: root.show4decks ? Theme.space1 : 0
                        top: parent.top
                    }
                }
                Skin.MixerColumn {
                    id: mixerDeck3

                    group: root.groups[2]
                    height: eqDeck2.height
                    visible: root.show4decks
                    width: root.stripWidth

                    anchors {
                        left: parent.left
                        top: eqDeck3.bottom
                    }
                }
                Skin.MixerColumn {
                    id: mixerDeck1

                    group: root.groups[0]
                    height: eqDeck1.height
                    width: root.stripWidth

                    anchors {
                        left: eqDeck1.right
                        leftMargin: Theme.space1
                        top: parent.top
                    }
                }
                Skin.MixerColumn {
                    id: mixerDeck2

                    group: root.groups[1]
                    height: eqDeck2.height
                    width: root.stripWidth

                    anchors {
                        right: eqDeck2.left
                        rightMargin: Theme.space1
                        top: parent.top
                    }
                }
                Skin.MixerColumn {
                    id: mixerDeck4

                    group: root.groups[3]
                    height: eqDeck2.height
                    visible: root.show4decks
                    width: root.stripWidth

                    anchors {
                        right: parent.right
                        top: eqDeck4.bottom
                    }
                }
                Skin.EqColumn {
                    id: eqDeck2

                    group: root.groups[1]
                    width: root.stripWidth

                    anchors {
                        right: parent.right
                        rightMargin: root.show4decks ? Theme.space1 : 0
                        top: parent.top
                    }
                }
                Skin.EqColumn {
                    id: eqDeck4

                    group: root.groups[3]
                    visible: root.show4decks
                    width: root.stripWidth

                    anchors {
                        right: parent.right
                        top: parent.top
                    }
                }
            }
        }
        Item {
            id: crossfader

            height: 44
            width: root.implicitWidth

            Rectangle {
                id: crossfaderSurface

                anchors.fill: parent
                anchors.margins: Theme.space1
                border.color: Theme.border
                border.width: 1
                color: Theme.surface0
                radius: Theme.radiusSm

                // Center detent marker behind the fader.
                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.verticalCenter: parent.verticalCenter
                    color: Theme.borderStrong
                    height: 10
                    width: 1
                }
                GridLayout {
                    id: leftDeckAssignment

                    anchors.left: parent.left
                    anchors.leftMargin: Theme.space1
                    anchors.verticalCenter: parent.verticalCenter
                    columnSpacing: 2
                    rowSpacing: 2
                    width: root.show4decks ? 34 : 16

                    Repeater {
                        model: root.show4decks ? 4 : 2

                        Item {
                            required property int index

                            Layout.column: show4decks ? index % 2 : 0
                            Layout.row: root.show4decks ? parseInt(index / 2) : index
                            implicitHeight: 16
                            implicitWidth: 16

                            Skin.ControlButton {
                                id: deckButton

                                Accessible.name: qsTr("Assign crossfader to deck %1").arg(index + 1)
                                activeColor: Theme.textOnAccent
                                anchors.fill: parent
                                group: `[Channel${index + 1}]`
                                key: "orientation_left"
                                text: qsTr("%1").arg(index + 1)
                                toggleable: true
                            }
                        }
                    }
                }
                Skin.ControlFader {
                    id: crossfaderSlider

                    anchors.left: leftDeckAssignment.right
                    anchors.leftMargin: Theme.space1
                    anchors.right: rightDeckAssignment.left
                    anchors.rightMargin: Theme.space1
                    anchors.verticalCenter: parent.verticalCenter
                    bar.color: Theme.accent
                    bar.start: 0.5
                    bg: Theme.imgCrossfaderBackground
                    fg: Theme.imgCrossfaderHandle
                    group: "[Master]"
                    key: "crossfader"
                    orientation: Qt.Horizontal

                    handleImage {
                        height: 28
                    }
                }
                GridLayout {
                    id: rightDeckAssignment

                    anchors.right: parent.right
                    anchors.rightMargin: Theme.space1
                    anchors.verticalCenter: parent.verticalCenter
                    columnSpacing: 2
                    rowSpacing: 2
                    width: root.show4decks ? 34 : 16

                    Repeater {
                        model: root.show4decks ? 4 : 2

                        Item {
                            required property int index

                            Layout.column: show4decks ? index % 2 : 0
                            Layout.row: root.show4decks ? parseInt(index / 2) : index
                            implicitHeight: 16
                            implicitWidth: 16

                            Skin.ControlButton {
                                id: deckButtonRight

                                Accessible.name: qsTr("Assign crossfader to deck %1").arg(index + 1)
                                activeColor: Theme.textOnAccent
                                anchors.fill: parent
                                group: `[Channel${index + 1}]`
                                key: "orientation_right"
                                text: qsTr("%1").arg(index + 1)
                                toggleable: true
                            }
                        }
                    }
                }
            }
        }
    }
}
