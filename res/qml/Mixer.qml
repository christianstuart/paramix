import "." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import QtQuick.Layouts
import "Mixer" as MixerComponents
import "Theme"

/// Channel mixer for two or four decks.
///
/// The mixer keeps a single flat surface. The left and right channel pairs are
/// separated by one hairline so the strips read as two banks, and the deck
/// assignment buttons for the crossfader live in a recessed strip at the
/// bottom. A master section underneath carries the main/booth outputs, the
/// headphone controls and the main stereo meter.
Item {
    id: root

    required property var groups
    property bool show4decks: false

    readonly property int stripWidth: 44

    implicitHeight: content.height + sectionLabel.height
    implicitWidth: 196

    Skin.SectionBackground {
        anchors.fill: parent
    }
    Column {
        anchors.fill: parent

        // Console section label: uppercase micro-type, Rekordbox-style.
        Item {
            id: sectionLabel

            height: 22
            width: root.implicitWidth

            Text {
                anchors.centerIn: parent
                color: Theme.textMuted
                font.family: Theme.fontFamily
                font.letterSpacing: Theme.letterSpacingCaps
                font.pixelSize: Theme.fontSizeXs
                font.weight: Theme.fontWeightDemiBold
                text: qsTr("MIXER")
            }
            Rectangle {
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.leftMargin: Theme.space2
                anchors.right: parent.right
                anchors.rightMargin: Theme.space2
                color: Theme.border
                height: 1
            }
        }

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
                        name: "4decks"
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
                    color: Theme.borderStrong
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
    }
}
