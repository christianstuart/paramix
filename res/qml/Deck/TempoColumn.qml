import ".." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts
import QtQuick.Shapes
import "../Theme"

/// Tempo strip: BPM editor, rate keys, key, pitch fader, sync and range.
///
/// Every control is 16-24 px high and shares the neutral-surface /
/// accent-active treatment used by the toolbar. Key colours tint the key
/// label only, not the whole button, so the strip stays calm at rest.
GridLayout {
    id: root

    /// Beside the overview (Traktor layout): readout and keylock row on the
    /// left, the fader and nudge keys on the right; SYNC and range live in the
    /// deck's transport row instead.
    property bool compact: false
    required property var currentTrack
    /// True while the BPM readout is replaced by the text field.
    property bool editingBpm: false
    required property string group
    readonly property bool loaded: trackLoadedControl.value > 0

    function beginBpmEdit() {
        if (!root.loaded || localBpmControl.value <= 0) {
            return;
        }
        bpmInput.text = bpmReadout.bpm.toFixed(2);
        root.editingBpm = true;
        bpmInput.forceActiveFocus();
        bpmInput.selectAll();
    }
    function cancelBpmEdit() {
        root.editingBpm = false;
    }
    function commitBpmEdit() {
        if (!root.editingBpm) {
            return;
        }
        const value = Number(bpmInput.text.trim());
        // Out-of-range or non-numeric input reverts, so a typo can never
        // rewrite the tempo to a nonsense value.
        if (isFinite(value) && value >= 20 && value <= 300) {
            bpmControl.value = value;
        }
        root.editingBpm = false;
    }

    columnSpacing: Theme.space1
    columns: root.compact ? 2 : 1
    rowSpacing: Theme.space1

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
    Mixxx.ControlProxy {
        id: rateDefaultControl

        group: root.group
        key: "rate_set_default"
    }
    Mixxx.ControlProxy {
        id: localBpmControl

        group: root.group
        key: "local_bpm"
    }
    // Two-line readout stacked over the fader, the deck-header tempo block of
    // the reference layout. The block owns the edit overlays so the inner
    // Column stays a pure text stack.
    Item {
        id: readoutBlock

        Layout.column: 0
        Layout.fillWidth: true
        Layout.row: 0
        implicitHeight: bpmReadout.implicitHeight + pitchRatio.implicitHeight
        opacity: trackLoadedControl.value ? 1 : 0

        Behavior on opacity {
            NumberAnimation {
                duration: Theme.motionNormal
            }
        }

        Text {
            id: bpmReadout

            readonly property real bpm: bpmControl.value

            Accessible.name: qsTr("%1 BPM").arg(bpmReadout.text)
            Accessible.role: Accessible.StaticText
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            color: Theme.textPrimary
            font.family: Theme.fontFamilyMono
            font.pixelSize: Theme.fontSizeMd
            font.weight: Theme.fontWeightDemiBold
            horizontalAlignment: Text.AlignHCenter
            text: bpm.toFixed(2)

            Mixxx.ControlProxy {
                id: bpmControl

                group: root.group
                key: "bpm"
            }
        }
        Text {
            id: pitchRatio

            readonly property real ratio: ((rateRatioControl.value - 1) * 100).toPrecision(2)

            Accessible.name: qsTr("Pitch %1").arg(pitchRatio.text)
            Accessible.role: Accessible.StaticText
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: bpmReadout.bottom
            color: Theme.textMuted
            font.family: Theme.fontFamilyMono
            font.pixelSize: Theme.fontSizeXs
            horizontalAlignment: Text.AlignHCenter
            text: ((ratio > 0) ? "+" + ratio.toFixed(2) : ratio.toFixed(2)) + "%"

            Mixxx.ControlProxy {
                id: rateRatioControl

                group: root.group
                key: "rate_ratio"
            }
        }
        // Clicking the BPM readout opens the editor. Return applies and Esc
        // reverts; losing focus also reverts so a typed tempo is never left
        // half-applied.
        MouseArea {
            id: bpmClickArea

            Accessible.description: qsTr("Set the desired tempo (rate) in BPM")
            Accessible.name: qsTr("Edit tempo")
            Accessible.role: Accessible.Button
            ToolTip.delay: 500
            ToolTip.text: qsTr("Set tempo (rate)")
            ToolTip.visible: bpmClickArea.containsMouse
            acceptedButtons: Qt.LeftButton
            activeFocusOnTab: true
            anchors.fill: bpmReadout
            cursorShape: Qt.IBeamCursor
            enabled: root.loaded && localBpmControl.value > 0 && !root.editingBpm
            hoverEnabled: true

            Keys.onReturnPressed: root.beginBpmEdit()
            Keys.onSpacePressed: root.beginBpmEdit()
            onClicked: root.beginBpmEdit()
        }
        // Focus is on the click area until the editor opens, so the ring is
        // what tells a keyboard user the readout is selected.
        Rectangle {
            anchors.fill: bpmReadout
            border.color: Theme.focusRing
            border.width: 1
            color: "transparent"
            radius: Theme.radiusSm
            visible: bpmClickArea.activeFocus
        }
        Rectangle {
            id: bpmEditor

            anchors.fill: bpmReadout
            border.color: Theme.focusRing
            border.width: 1
            color: Theme.surface0
            radius: Theme.radiusSm
            visible: root.editingBpm

            TextInput {
                id: bpmInput

                Accessible.description: qsTr("Set tempo (rate) in BPM. Return applies, Esc reverts.")
                Accessible.name: qsTr("Tempo")
                anchors.fill: parent
                color: Theme.accent
                font.family: Theme.fontFamilyMono
                font.pixelSize: Theme.fontSizeMd
                font.weight: Theme.fontWeightDemiBold
                horizontalAlignment: TextInput.AlignHCenter
                inputMethodHints: Qt.ImhFormattedNumbersOnly
                selectByMouse: true
                verticalAlignment: TextInput.AlignVCenter

                Keys.onEscapePressed: root.cancelBpmEdit()
                onAccepted: root.commitBpmEdit()
                onActiveFocusChanged: {
                    if (!activeFocus && root.editingBpm) {
                        root.commitBpmEdit();
                    }
                }
            }
        }
        // Clicking the rate readout resets the speed to the track tempo.
        MouseArea {
            id: rateResetArea

            Accessible.description: qsTr("Reset the playback rate to the track tempo")
            Accessible.name: qsTr("Reset tempo")
            Accessible.role: Accessible.Button
            ToolTip.delay: 500
            ToolTip.text: qsTr("Reset tempo")
            ToolTip.visible: rateResetArea.containsMouse
            acceptedButtons: Qt.LeftButton
            activeFocusOnTab: true
            anchors.fill: pitchRatio
            cursorShape: Qt.PointingHandCursor
            enabled: root.loaded
            hoverEnabled: true

            Keys.onReturnPressed: rateDefaultControl.trigger()
            Keys.onSpacePressed: rateDefaultControl.trigger()
            onClicked: rateDefaultControl.trigger()
        }
        Rectangle {
            anchors.fill: pitchRatio
            border.color: Theme.focusRing
            border.width: 1
            color: "transparent"
            radius: Theme.radiusSm
            visible: rateResetArea.activeFocus
        }
    }
    RowLayout {
        Layout.column: 0
        Layout.fillWidth: true
        Layout.preferredHeight: 20
        Layout.row: 1
        spacing: Theme.space1

        Skin.ControlButton {
            id: pitchDownButton

            Accessible.name: qsTr("Pitch down")
            Layout.fillHeight: true
            Layout.preferredWidth: 18
            activeColor: Theme.textOnAccent
            group: root.group
            implicitHeight: 20
            implicitWidth: 18
            key: "pitch_down"

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

            // Fine pitch on right click, the coarse key on the left.
            MouseArea {
                acceptedButtons: Qt.RightButton
                anchors.fill: parent
                enabled: root.loaded

                onPressed: pitchDownSmall.trigger()

                Mixxx.ControlProxy {
                    id: pitchDownSmall

                    group: root.group
                    key: "pitch_down_small"
                }
            }
        }
        // Keylock key. Enabled: the face is filled with the track key's own
        // colour and shows the key, so the strip carries the Camelot colour
        // instead of another accent block. Disabled: a plain grey note.
        Skin.Button {
            id: pitchKey

            /// Keylock engaged *and* the deck knows the track key, so the face
            /// can carry the Camelot colour.
            readonly property bool active: pitchKey.lockOn && trackLoadedControl.value != 0 && keyCO.value > 0

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
            readonly property color keyColor: pitchKey.active ? colorsMap[keyCO.value - 1] : "transparent"
            /// Keylock is engaged (note icon palette depends on this).
            readonly property bool lockOn: keyBehavior.isActive
            readonly property variant textMap: ["1d", "8d", "3d", "10d", "5d", "12d", "7d", "2d", "9d", "4d", "11d", "6d", "10m", "5m", "12m", "7m", "2m", "9m", "4m", "11m", "6m", "1m", "8m", "3m"]

            Accessible.name: qsTr("Keylock")
            Layout.fillHeight: true
            Layout.fillWidth: true
            highlight: false
            implicitHeight: 20

            background: Rectangle {
                border.color: pitchKey.active ? Qt.lighter(pitchKey.keyColor, 1.15) : Theme.border
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
                color: pitchKey.active ? Theme.surface0 : pitchKey.lockOn ? Theme.accent : Theme.textMuted
                font.capitalization: Font.MixedCase
                font.family: Theme.fontFamily
                font.pixelSize: pitchKey.active ? Theme.fontSizeXs : Theme.fontSizeMd
                font.weight: Theme.fontWeightDemiBold
                horizontalAlignment: Text.AlignHCenter
                text: pitchKey.active ? pitchKey.textMap[keyCO.value - 1] : "♪"
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

            Accessible.name: qsTr("Pitch up")
            Layout.fillHeight: true
            Layout.preferredWidth: 18
            activeColor: Theme.textOnAccent
            group: root.group
            implicitHeight: 20
            implicitWidth: 18
            key: "pitch_up"

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

            // Fine pitch on right click, the coarse key on the left.
            MouseArea {
                acceptedButtons: Qt.RightButton
                anchors.fill: parent
                enabled: root.loaded

                onPressed: pitchUpSmall.trigger()

                Mixxx.ControlProxy {
                    id: pitchUpSmall

                    group: root.group
                    key: "pitch_up_small"
                }
            }
        }
    }
    RowLayout {
        Layout.column: root.compact ? 1 : 0
        Layout.fillHeight: true
        Layout.fillWidth: !root.compact
        Layout.preferredWidth: root.compact ? 36 : -1
        Layout.row: root.compact ? 0 : 2
        Layout.rowSpan: root.compact ? 2 : 1
        spacing: Theme.space1

        Skin.ControlFader {
            id: rateSlider

            Accessible.name: qsTr("Tempo")
            Layout.fillHeight: true
            Layout.fillWidth: true
            bar.color: Theme.accent
            bar.margin: 2
            bar.start: 0.5
            bar.width: 3
            group: root.group
            implicitHeight: 60
            implicitWidth: 16
            key: "rate"
            showDefaultHandle: false

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
        // Speed keys sit beside the fader rather than in two rows above it, so
        // the fader keeps its travel without hiding any of them.
        ColumnLayout {
            Layout.fillHeight: true
            Layout.preferredWidth: 14
            spacing: 2

            RateKey {
                description: qsTr("Lowers the track playback speed (tempo). Right click: lowers playback speed in small steps.")
                fineKey: "rate_perm_down_small"
                key: "rate_perm_down"
                label: "−"
                title: qsTr("Slow Down")
            }
            RateKey {
                description: qsTr("Raises the track playback speed (tempo). Right click: raises playback speed in small steps.")
                fineKey: "rate_perm_up_small"
                key: "rate_perm_up"
                label: "+"
                title: qsTr("Speed Up")
            }
            RateKey {
                description: qsTr("Holds playback speed lower while active (tempo). Right click: holds playback speed lower (small amount) while active.")
                fineKey: "rate_temp_down_small"
                key: "rate_temp_down"
                label: "◀"
                title: qsTr("Slow Down Temporarily (Nudge)")
            }
            RateKey {
                description: qsTr("Holds playback speed higher while active (tempo). Right click: holds playback speed higher (small amount) while active.")
                fineKey: "rate_temp_up_small"
                key: "rate_temp_up"
                label: "▶"
                title: qsTr("Speed Up Temporarily (Nudge)")
            }
        }
    }
    Skin.SyncButton {
        id: syncButton

        Accessible.name: syncButton.highlight ? qsTr("Disable sync") : qsTr("Enable sync")
        Layout.column: 0
        Layout.fillWidth: true
        Layout.preferredHeight: 20
        Layout.row: 3
        group: root.group
        visible: !root.compact
    }
    Skin.RangeButton {
        id: rangeButton

        Accessible.name: qsTr("Tempo range")
        Layout.column: 0
        Layout.fillWidth: true
        Layout.preferredHeight: 20
        Layout.row: 4
        group: root.group
        visible: !root.compact
    }

    /// One speed key. Left click steps once, right click steps in the fine
    /// increment and is held for the momentary nudge keys.
    component RateKey: Skin.Button {
        id: rateKey

        property string description: ""
        required property string fineKey
        required property string key
        required property string label
        required property string title

        Accessible.description: rateKey.description
        Accessible.name: rateKey.title
        Layout.fillHeight: true
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        ToolTip.delay: 500
        ToolTip.text: rateKey.title
        ToolTip.visible: rateKey.hovered
        activeFocusOnTab: true
        enabled: root.loaded
        implicitHeight: 16
        text: rateKey.label

        contentItem: Text {
            anchors.fill: parent
            anchors.leftMargin: 1
            anchors.rightMargin: 1
            color: rateKey.labelColor
            elide: Text.ElideRight
            font.capitalization: Font.AllUppercase
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeXs
            font.weight: Theme.fontWeightDemiBold
            horizontalAlignment: Text.AlignHCenter
            text: rateKey.label
            verticalAlignment: Text.AlignVCenter
        }

        onCanceled: controlBehavior.releasePrimary()
        onPressed: controlBehavior.pressPrimary()
        onReleased: controlBehavior.releasePrimary()

        Skin.ControlProxyButtonBehavior {
            id: controlBehavior

            group: root.group
            handlePointerInput: false
            key: rateKey.key
        }
        MouseArea {
            acceptedButtons: Qt.RightButton
            anchors.fill: parent
            enabled: root.loaded

            onCanceled: fineControl.value = 0
            onPressed: fineControl.value = 1
            onReleased: fineControl.value = 0

            Mixxx.ControlProxy {
                id: fineControl

                group: root.group
                key: rateKey.fineKey
            }
        }
    }
}
