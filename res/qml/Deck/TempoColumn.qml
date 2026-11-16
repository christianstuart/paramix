import ".." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts
import QtQuick.Shapes
import "../Theme"

/// Tempo strip: pitch nudge, key, pitch fader, sync and range.
///
/// Every control is 24 px high and shares the neutral-surface / accent-active
/// treatment used by the toolbar. Key colours tint the key label only, not the
/// whole button, so the strip stays calm at rest.
ColumnLayout {
    id: root

    required property var currentTrack
    required property string group

    /// BPM and pitch live in the jog hub unless the user moved them above the
    /// fader in Customize Paramix.
    readonly property bool bpmInJog: Mixxx.Config.paramixBpmInJog !== false

    spacing: Theme.space1

    Mixxx.ControlProxy {
        id: keyCO

        group: root.group
        key: "key"
    }
    Mixxx.ControlProxy {
        id: trackLoadedControl

        group: root.group
        key: "track_loaded"
    }
    // Two-line readout stacked over the fader, the deck-header tempo block of
    // the reference layout.
    Column {
        Layout.fillWidth: true
        opacity: trackLoadedControl.value ? 1 : 0
        spacing: 0
        visible: !root.bpmInJog

        Behavior on opacity {
            NumberAnimation {
                duration: Theme.motionNormal
            }
        }

        Text {
            id: bpmReadout

            readonly property real bpm: bpmControl.value
            readonly property int precision: 2

            width: parent.width
            color: Theme.textPrimary
            font.family: Theme.fontFamilyMono
            font.pixelSize: Theme.fontSizeMd
            font.weight: Theme.fontWeightDemiBold
            horizontalAlignment: Text.AlignHCenter
            text: `${Math.round(bpm)}.${((bpm % 1) * Math.pow(10, precision)).toFixed().padEnd(precision, "0")}`

            Accessible.name: qsTr("%1 BPM").arg(bpmReadout.text)
            Accessible.role: Accessible.StaticText

            Mixxx.ControlProxy {
                id: bpmControl

                group: root.group
                key: "bpm"
            }
        }
        Text {
            id: pitchRatio

            readonly property real ratio: ((rateRatioControl.value - 1) * 100).toPrecision(2)

            width: parent.width
            color: Theme.textMuted
            font.family: Theme.fontFamilyMono
            font.pixelSize: Theme.fontSizeXs
            horizontalAlignment: Text.AlignHCenter
            text: ((ratio > 0) ? "+" + ratio.toFixed(2) : ratio.toFixed(2)) + "%"

            Accessible.name: qsTr("Pitch %1").arg(pitchRatio.text)
            Accessible.role: Accessible.StaticText

            Mixxx.ControlProxy {
                id: rateRatioControl

                group: root.group
                key: "rate_ratio"
            }
        }
    }
    RowLayout {
        Layout.fillWidth: true
        Layout.preferredHeight: 20
        spacing: Theme.space1

        Skin.ControlButton {
            id: pitchDownButton

            Layout.fillHeight: true
            Layout.preferredWidth: 18
            activeColor: Theme.textOnAccent
            group: root.group
            implicitHeight: 20
            implicitWidth: 18
            key: "pitch_down"

            Accessible.name: qsTr("Pitch down")

            contentItem: Item {
                anchors.fill: parent

                Shape {
                    anchors.centerIn: parent
                    antialiasing: true
                    height: 10
                    layer.enabled: true
                    layer.samples: 4
                    width: 10

                    ShapePath {
                        fillColor: pitchDownButton.highlight ? Theme.textOnAccent : (pitchDownButton.hovered ? Theme.textPrimary : Theme.textSecondary)
                        startX: 0
                        startY: 5
                        strokeColor: 'transparent'

                        PathLine {
                            x: 10
                            y: 0
                        }
                        PathLine {
                            x: 10
                            y: 10
                        }
                        PathLine {
                            x: 0
                            y: 5
                        }
                    }
                }
            }
        }
        // Keylock key. Enabled: the face is filled with the track key's own
        // colour and shows the key, so the strip carries the Camelot colour
        // instead of another accent block. Disabled: a plain grey note.
        Skin.Button {
            id: pitchKey

            // FIXME: the following maps are copied from the S4 mapping. Once the
            // interface setting PR is merged, we should use the palette.
            readonly property variant colorsMap: ["#b960a2" // 1d
                , "#9fc516" // 8d
                , "#527fc0" // 3d
                , "#f28b2e" // 10d
                , "#5bc1cf" // 5d
                , "#e84c4d" // 12d
                , "#73b629" // 7d
                , "#8269ab" // 2d
                , "#fdd615" // 9d
                , "#3cc0f0" // 4d
                , "#4cb686" // 11d
                , "#4cb686" // 6d
                , "#f5a158" // 10m
                , "#7bcdd9" // 5m
                , "#ed7171" // 12m
                , "#8fc555" // 7m
                , "#9b86be" // 2m
                , "#fcdf45" // 9m
                , "#63cdf4" // 4m
                , "#f1845f" // 11m
                , "#70c4a0" // 6m
                , "#c680b6" // 1m
                , "#b2d145" // 8m
                , "#7499cd" // 3m
            ]
            readonly property variant textMap: ["1d", "8d", "3d", "10d", "5d", "12d", "7d", "2d", "9d", "4d", "11d", "6d", "10m", "5m", "12m", "7m", "2m", "9m", "4m", "11m", "6m", "1m", "8m", "3m"]
            /// Keylock is engaged (note icon palette depends on this).
            readonly property bool lockOn: keyBehavior.isActive
            /// Keylock engaged *and* the deck knows the track key, so the face
            /// can carry the Camelot colour.
            readonly property bool active: pitchKey.lockOn
                    && trackLoadedControl.value != 0
                    && keyCO.value > 0
            readonly property color keyColor: pitchKey.active
                    ? colorsMap[keyCO.value]
                    : "transparent"

            Layout.fillHeight: true
            Layout.fillWidth: true
            highlight: false
            implicitHeight: 20

            Accessible.name: qsTr("Keylock")

            background: Rectangle {
                border.color: pitchKey.active
                        ? Qt.lighter(pitchKey.keyColor, 1.15)
                        : Theme.border
                border.width: 1
                color: pitchKey.active ? pitchKey.keyColor : Theme.surface2
                radius: Theme.radiusSm

                Behavior on color {
                    ColorAnimation {
                        duration: Theme.motionFast
                    }
                }
            }
            contentItem: Text {
                // Locked without a track keeps the note, in the accent, so the
                // engaged state is still visible on an empty deck.
                color: pitchKey.active
                       ? Theme.surface0
                       : pitchKey.lockOn ? Theme.accent : Theme.textMuted
                font.capitalization: Font.MixedCase
                font.family: Theme.fontFamily
                font.pixelSize: pitchKey.active ? Theme.fontSizeXs : Theme.fontSizeMd
                font.weight: Theme.fontWeightDemiBold
                horizontalAlignment: Text.AlignHCenter
                text: pitchKey.active ? pitchKey.textMap[keyCO.value] : "♪"
                verticalAlignment: Text.AlignVCenter
            }
            onCanceled: keyBehavior.releasePrimary()
            onPressed: keyBehavior.pressPrimary()
            onReleased: keyBehavior.releasePrimary()

            Skin.ControlProxyButtonBehavior {
                id: keyBehavior

                group: root.group
                handlePointerInput: false
                key: "keylock"
                toggleable: true
            }
        }
        Skin.ControlButton {
            id: pitchUpButton

            Layout.fillHeight: true
            Layout.preferredWidth: 18
            activeColor: Theme.textOnAccent
            group: root.group
            implicitHeight: 20
            implicitWidth: 18
            key: "pitch_up"

            Accessible.name: qsTr("Pitch up")

            contentItem: Item {
                anchors.fill: parent

                Shape {
                    anchors.centerIn: parent
                    antialiasing: true
                    height: 10
                    layer.enabled: true
                    layer.samples: 4
                    width: 10

                    ShapePath {
                        fillColor: pitchUpButton.highlight ? Theme.textOnAccent : (pitchUpButton.hovered ? Theme.textPrimary : Theme.textSecondary)
                        startX: 0
                        startY: 0
                        strokeColor: 'transparent'

                        PathLine {
                            x: 10
                            y: 5
                        }
                        PathLine {
                            x: 0
                            y: 10
                        }
                        PathLine {
                            x: 0
                            y: 0
                        }
                    }
                }
            }
        }
    }
    Skin.ControlFader {
        id: rateSlider

        Layout.fillHeight: true
        Layout.fillWidth: true
        implicitHeight: 80
        implicitWidth: 16
        showDefaultHandle: false
        bar.color: Theme.accent
        bar.margin: 2
        bar.start: 0.5
        bar.width: 3
        group: root.group
        key: "rate"

        Accessible.name: qsTr("Tempo")

        background: Rectangle {
            // Mixxx.Controls.Slider hosts custom backgrounds in a fill item;
            // without anchors the Rectangle would collapse to 0 x 0 and the
            // rail would never paint.
            anchors.fill: parent
            color: "transparent"

            // Recessed centre well so the fader reads as a hardware rail even
            // when the handle is parked at 0 %.
            Rectangle {
                anchors.centerIn: parent
                border.color: Theme.border
                border.width: 1
                color: Theme.surface0
                height: parent.height
                radius: Theme.radiusSm
                width: 10
            }
            Rectangle {
                anchors.centerIn: parent
                color: Theme.borderStrong
                height: parent.height
                radius: 1
                width: 3
            }
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.verticalCenter: parent.verticalCenter
                color: Theme.borderStrong
                height: 1
                width: 9
            }
        }
        handle: Rectangle {
            border.color: Theme.borderStrong
            border.width: 1
            color: Theme.textPrimary
            height: 8
            radius: Theme.radiusXs
            width: 30
            x: Math.round((rateSlider.width - width) / 2)
            y: Math.round(rateSlider.visualPosition * (rateSlider.height - height))
        }
    }
    Skin.SyncButton {
        id: syncButton

        Layout.fillWidth: true
        Layout.preferredHeight: 20
        group: root.group

        Accessible.name: syncButton.highlight ? qsTr("Disable sync") : qsTr("Enable sync")
    }
    Skin.RangeButton {
        id: rangeButton

        Layout.fillWidth: true
        Layout.preferredHeight: 20
        group: root.group

        Accessible.name: qsTr("Tempo range")
    }
}
