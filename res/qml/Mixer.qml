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

    implicitHeight: content.height + crossfader.height + outputSection.height + sectionLabel.height
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
        Item {
            id: crossfader

            height: 44
            width: root.implicitWidth

            Rectangle {
                id: crossfaderSurface

                anchors.fill: parent
                anchors.margins: Theme.space1
                border.color: Theme.borderStrong
                border.width: 1
                color: Theme.surface0
                radius: Theme.radiusSm

                // Lit top edge inside the recessed fader well.
                Rectangle {
                    anchors.left: parent.left
                    anchors.leftMargin: 1
                    anchors.right: parent.right
                    anchors.rightMargin: 1
                    anchors.top: parent.top
                    anchors.topMargin: 1
                    color: Theme.topHighlight
                    height: 1
                }

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

                    Accessible.name: qsTr("Crossfader")
                    Accessible.role: Accessible.Slider

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
        // Master section: main outputs (gain, balance, booth trim) over the
        // headphone controls (gain, cue mix, split), with the stereo meter
        // spanning both bands. Tile width follows the column width; a band
        // wraps to another row instead of shrinking a tile below tileMinWidth,
        // so captions never elide mid-word.
        Item {
            id: outputSection

            /// Narrowest tile that still fits "BOOTH" / "SPLIT" at fontScale 1.
            readonly property int tileMinWidth: 52
            readonly property int meterWidth: 10
            readonly property int gap: Theme.space1
            readonly property int innerWidth: Math.max(0, width - 2 * Theme.space1)
            /// Width left for the tiles once the meter column is reserved.
            readonly property int bandWidth: Math.max(tileMinWidth, innerWidth - meterWidth - gap)
            readonly property int columns: bandWidth >= 3 * tileMinWidth + 2 * gap ? 3
                    : bandWidth >= 2 * tileMinWidth + gap ? 2
                    : 1
            readonly property real tileWidth: Math.floor((bandWidth - (columns - 1) * gap) / columns)
            readonly property int tileHeight: 54

            height: 1 + Theme.space1 + bands.height + Theme.space1
            width: root.width

            // Hairline between the crossfader well and the master section.
            Rectangle {
                id: outputSeparator

                anchors.left: parent.left
                anchors.leftMargin: Theme.space2
                anchors.right: parent.right
                anchors.rightMargin: Theme.space2
                anchors.top: parent.top
                color: Theme.border
                height: 1
            }
            Item {
                id: bands

                anchors.left: parent.left
                anchors.leftMargin: Theme.space1
                anchors.right: parent.right
                anchors.rightMargin: Theme.space1
                anchors.top: outputSeparator.bottom
                anchors.topMargin: Theme.space1
                height: bandRow.height

                Row {
                    id: bandRow

                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    spacing: outputSection.gap

                    Column {
                        id: bandColumn

                        spacing: outputSection.gap
                        width: outputSection.bandWidth

                        // Main output: gain, balance and booth trim.
                        Grid {
                            id: mainBand

                            columns: outputSection.columns
                            spacing: outputSection.gap
                            width: parent.width

                            MixerComponents.OutputTile {
                                group: "[Master]"
                                height: outputSection.tileHeight
                                key: "gain"
                                label: qsTr("MAIN")
                                readoutMode: "gain"
                                width: outputSection.tileWidth
                            }
                            MixerComponents.OutputTile {
                                group: "[Master]"
                                height: outputSection.tileHeight
                                key: "balance"
                                label: qsTr("BAL")
                                readoutMode: "balance"
                                width: outputSection.tileWidth
                            }
                            MixerComponents.OutputTile {
                                enabled: boothEnabledControl.value > 0
                                group: "[Master]"
                                height: outputSection.tileHeight
                                key: "booth_gain"
                                label: qsTr("BOOTH")
                                opacity: enabled ? 1 : 0.45
                                readoutMode: "gain"
                                width: outputSection.tileWidth
                            }
                        }

                        // Hairline between the main and the headphone controls.
                        Rectangle {
                            color: Theme.border
                            height: 1
                            width: parent.width
                        }

                        // Headphone section: gain, cue/main mix and split cue.
                        Grid {
                            id: headBand

                            columns: outputSection.columns
                            spacing: outputSection.gap
                            width: parent.width

                            MixerComponents.OutputTile {
                                group: "[Master]"
                                height: outputSection.tileHeight
                                key: "headGain"
                                label: qsTr("HEAD")
                                readoutMode: "gain"
                                width: outputSection.tileWidth
                            }
                            MixerComponents.OutputTile {
                                group: "[Master]"
                                height: outputSection.tileHeight
                                key: "headMix"
                                label: qsTr("MIX")
                                readoutMode: "mix"
                                width: outputSection.tileWidth
                            }
                            Skin.ControlButton {
                                activeColor: Theme.textOnAccent
                                group: "[Master]"
                                height: outputSection.tileHeight
                                key: "headSplit"
                                text: qsTr("Split")
                                toggleable: true
                                width: outputSection.tileWidth
                            }
                        }
                    }

                    // Master meter spans both bands, with a clip LED per channel.
                    Item {
                        id: outputMeter

                        height: bandColumn.height
                        width: outputSection.meterWidth

                        Row {
                            anchors.fill: parent
                            spacing: 2

                            Column {
                                height: parent.height
                                spacing: 2
                                width: 4

                                Rectangle {
                                    color: peakLeft.value > 0 ? Theme.vuRed : Theme.surface2
                                    height: 3
                                    radius: 1
                                    width: 4
                                }
                                Skin.VuMeter {
                                    group: "[Main]"
                                    height: Math.max(0, parent.height - 5)
                                    key: "vu_meter_left"
                                    width: 4
                                }
                            }
                            Column {
                                height: parent.height
                                spacing: 2
                                width: 4

                                Rectangle {
                                    color: peakRight.value > 0 ? Theme.vuRed : Theme.surface2
                                    height: 3
                                    radius: 1
                                    width: 4
                                }
                                Skin.VuMeter {
                                    group: "[Main]"
                                    height: Math.max(0, parent.height - 5)
                                    key: "vu_meter_right"
                                    width: 4
                                }
                            }
                        }
                    }
                }
            }
            Mixxx.ControlProxy {
                id: boothEnabledControl

                group: "[Master]"
                key: "booth_enabled"
            }
            Mixxx.ControlProxy {
                id: peakLeft

                group: "[Main]"
                key: "peak_indicator_left"
            }
            Mixxx.ControlProxy {
                id: peakRight

                group: "[Main]"
                key: "peak_indicator_right"
            }
        }
    }
}
