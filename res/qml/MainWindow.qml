import "." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import QtQuick.Shapes
import Qt5Compat.GraphicalEffects
import "Mixer" as MixerComponents
import "ControllerMapping" as ControllerMappingComponents
import "Theme"

Item {
    id: root

    required property ApplicationWindow applicationWindow
    property alias menuBar: nativeApplicationMenuLoader.item

    property var focusedDeck: null
    // Layout visibility is driven by the View menu in the toolbar. The stored
    // state is kept independently of window size so shrinking the window only
    // hides the four-deck layout, it does not forget the preference.
    property bool fourDecksRequested: false
    property bool maximizeLibrary: false
    readonly property int numDecks: 4
    readonly property int numSamplers: 16
    readonly property bool show4decks: fourDecksRequested && root.height > 515
    /// Height the deck band reserves: the taller of the mixer and the deck
    /// rows. Four decks need two deck rows even when the mixer's EQ columns
    /// are short (three bands), so the decks cannot be squeezed below their
    /// minimum.
    readonly property int bandContentHeight: Math.max(mixer.implicitHeight,
            Theme.deckMinHeight * (root.show4decks ? 2 : 1))
    /// The deck/mixer band may never reserve more height than the window leaves
    /// under the toolbar. Without this the vertical split honours a minimum that
    /// is larger than the window and paints the band past the bottom edge
    /// instead of shrinking it.
    /// Stacked rows below the deck/mixer band: output bar, preview bar, input
    /// rack and samplers. They have to be reserved too, otherwise the band is
    /// capped at the deck height and the last row is laid out below the window.
    readonly property int bandChromeHeight: outputBar.height + previewDeckBar.height +
            inputRack.height + samplers.height
    readonly property int bandMinimumHeight: Math.min(root.bandContentHeight + root.bandChromeHeight,
            Math.max(0, root.height - Theme.topBarHeight))
    property bool showSamplers: false
    readonly property bool showMicrophoneInputs: showMicrophonesControl.value > 0
    readonly property int waveformOverviewTypeRgb: 2

    onMaximizeLibraryChanged: {
        if ((showMaximizedLibrary.value > 0) !== root.maximizeLibrary) {
            showMaximizedLibrary.value = root.maximizeLibrary ? 1 : 0;
        }
    }

    Loader {
        id: nativeApplicationMenuLoader

        active: Qt.platform.os === "osx" || (Mixxx.Application.supportsGlobalMenuBar && root.applicationWindow.visibility !== Window.FullScreen)

        sourceComponent: Skin.MainMenuBar {
            actions: applicationMenuActions
        }
    }
    Skin.ApplicationMenuCommands {
        id: applicationMenuCommands

        applicationWindow: root.applicationWindow

        onShowDeveloperToolsRequested: {
            devToolsWindow.show();
            devToolsWindow.raise();
            devToolsWindow.requestActivate();
        }
    }
    Skin.ApplicationMenuActions {
        id: applicationMenuActions

        applicationWindow: root.applicationWindow
        commands: applicationMenuCommands
        numberOfDecks: root.show4decks ? root.numDecks : 2

        onFocusLibrarySearchRequested: {
            if (library.item) {
                library.item.focusSearch();
            }
        }
    }
    Skin.LibraryScanSummaryDialog {
    }
    Mixxx.ControlProxy {
        group: "[App]"
        key: "num_decks"

        onInitializedChanged: {
            value = root.numDecks;
        }
    }
    Mixxx.ControlProxy {
        group: "[App]"
        key: "num_samplers"

        onInitializedChanged: {
            value = root.numSamplers;
        }
    }
    Mixxx.ControlProxy {
        group: "[Waveform]"
        key: "WaveformOverviewType"

        onInitializedChanged: {
            value = root.waveformOverviewTypeRgb;
        }
    }
    Mixxx.ControlProxy {
        id: showMaximizedLibrary

        group: "[Skin]"
        key: "show_maximized_library"

        onValueChanged: root.maximizeLibrary = value > 0
    }
    Mixxx.ControlProxy {
        id: showMicrophonesControl

        group: "[Skin]"
        key: "show_microphones"
    }
    Mixxx.ControlProxy {
        id: show4EffectUnitsControl

        group: "[Skin]"
        key: "show_4effectunits"
    }
    Mixxx.ControlProxy {
        id: showVinylControl

        group: "[Skin]"
        key: "show_vinylcontrol"
    }
    Mixxx.ControlProxy {
        id: showPreviewDecks

        group: "[Skin]"
        key: "show_preview_decks"
    }
    Mixxx.ControlProxy {
        id: previewDeckPlaying

        group: "[PreviewDeck1]"
        key: "play_indicator"
    }
    Mixxx.ControlProxy {
        id: numMicrophones
        group: "[App]"
        key: "num_microphones"
    }
    Mixxx.ControlProxy {
        id: numAuxiliaries
        group: "[App]"
        key: "num_auxiliaries"
    }
    Column {
        id: content

        anchors.fill: parent

        move: Transition {
            NumberAnimation {
                duration: Theme.motionNormal
                properties: "x,y"
            }
        }

        Rectangle {
            id: toolbar

            color: Theme.surface1
            height: Theme.topBarHeight
            width: parent.width

            // Main row: brand, centred master readouts and the layout keys.
            Item {
                id: mainToolbarRow

                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                height: Theme.toolbarHeight

                // Master tempo and tempo-source switch: centered in the bar so it
                // sits above the playback marker of the overview waveforms.
                MixerComponents.MasterTempoBox {
                    id: masterTempo

                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.verticalCenter: parent.verticalCenter
                    visible: root.width >= Theme.masterTempoMinWidth
                    z: 1
                }
                // Master output level, left of the tempo readout. Hidden on narrow
                // windows where it would collide with the layout toggles.
                MixerComponents.MasterLevelDisplay {
                    anchors.right: masterTempo.left
                    anchors.rightMargin: Theme.space4
                    anchors.verticalCenter: parent.verticalCenter
                    visible: root.width >= Theme.masterMeterMinWidth
                    z: 1
                }
                // Recording output, right of the tempo readout.
                MixerComponents.RecordingBox {
                    anchors.left: masterTempo.right
                    anchors.leftMargin: Theme.space4
                    anchors.verticalCenter: parent.verticalCenter
                    visible: root.width >= Theme.recordingBoxMinWidth
                    z: 1
                }
                // Flat matte top plate with a 1 px hairline bottom separator.
                Rectangle {
                    anchors.bottom: parent.bottom
                    color: Theme.border
                    height: 1
                    width: parent.width
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.space3
                    anchors.rightMargin: Theme.space2
                    spacing: Theme.space2
                    // Keep the View key and the status cluster above the centred
                    // master/recording overlays; at 1280-1447 px those overlays
                    // used to cover the View button, making the layout menu
                    // unreachable.
                    z: 2

                    // Brand plate: wordmark, deck status dots.
                    RowLayout {
                        spacing: Theme.space2

                        ColumnLayout {
                            spacing: 0

                            Text {
                                color: Theme.textPrimary
                                font.bold: true
                                font.family: Theme.fontFamilyDisplay
                                font.letterSpacing: 2.5
                                font.pixelSize: Theme.fontSizeSm
                                text: qsTr("PARAMIX")
                                verticalAlignment: Text.AlignVCenter
                            }
                        }
                        RowLayout {
                            spacing: 4

                            Layout.leftMargin: Theme.space2

                            Repeater {
                                model: [Theme.deckA, Theme.deckB, Theme.deckC, Theme.deckD]

                                Rectangle {
                                    Layout.alignment: Qt.AlignVCenter
                                    color: modelData
                                    height: 6
                                    opacity: 0.9
                                    radius: 3
                                    width: 6
                                }
                            }
                        }
                        Item {
                            implicitWidth: Theme.space2
                        }
                    }
                    Item {
                        Layout.fillWidth: true
                    }
                    // CPU / GPU / clock / battery. Each item is optional; the whole
                    // cluster hides before it can collide with the centred tempo.
                    TopBarStatus {
                        Layout.alignment: Qt.AlignVCenter
                        visible: root.width >= Theme.toolbarStatusMinWidth
                    }
                    // Layout visibility lives here instead of a row of toggles on
                    // the left: one key that opens a checkable menu.
                    Skin.Button {
                        id: viewButton

                        activeColor: Theme.textOnAccent
                        highlight: layoutMenu.visible
                        text: qsTr("View")

                        Accessible.name: qsTr("Show or hide interface panels")

                        onClicked: layoutMenu.visible ? layoutMenu.close() : layoutMenu.open()

                        Menu {
                            id: layoutMenu

                            parent: viewButton
                            width: 188
                            x: parent.width - width
                            y: parent.height + 4

                            background: Rectangle {
                                border.color: Theme.borderStrong
                                border.width: 1
                                color: Theme.surface2
                                radius: Theme.radiusMd
                            }
                            padding: Theme.space1

                            onAboutToShow: {
                                fourDecksItem.checked = root.fourDecksRequested;
                                libraryItem.checked = root.maximizeLibrary;
                                auxItem.checked = root.showMicrophoneInputs;
                                samplersItem.checked = root.showSamplers;
                                vinylItem.checked = showVinylControl.value > 0;
                                previewDeckItem.checked = showPreviewDecks.value > 0 ||
                                        (previewDeckBar.item && previewDeckBar.item.revealed) ||
                                        previewDeckPlaying.value > 0;
                                padFxItem.checked = padFxWindow.visible;
                                mappingItem.checked = controllerMappingWindow.visible;
                                fxUnitsItem.checked = show4EffectUnitsControl.value > 0;
                            }

                            Skin.CheckMenuItem {
                                id: fourDecksItem

                                checkable: true
                                enabled: root.height > 515
                                text: qsTr("Four decks")

                                onTriggered: root.fourDecksRequested = checked
                            }
                            Skin.CheckMenuItem {
                                id: libraryItem

                                checkable: true
                                text: qsTr("Library")

                                onTriggered: root.maximizeLibrary = checked
                            }
                            Skin.CheckMenuItem {
                                id: auxItem

                                checkable: true
                                text: qsTr("Mic / Aux")

                                onTriggered: showMicrophonesControl.value = checked ? 1 : 0
                            }
                            Skin.CheckMenuItem {
                                id: padFxItem

                                checkable: true
                                text: qsTr("Pad FX")

                                onTriggered: checked ? padFxWindow.show() : padFxWindow.close()
                            }
                            Skin.CheckMenuItem {
                                id: mappingItem

                                checkable: true
                                text: qsTr("Controller mapping")

                                onTriggered: checked ? controllerMappingWindow.show() : controllerMappingWindow.close()
                            }
                            Skin.CheckMenuItem {
                                id: fxUnitsItem

                                checkable: true
                                text: qsTr("FX 3-4")

                                onTriggered: show4EffectUnitsControl.value = checked ? 1 : 0
                            }
                            Skin.CheckMenuItem {
                                id: samplersItem

                                checkable: true
                                text: qsTr("Samplers")

                                onTriggered: root.showSamplers = checked
                            }
                            Skin.CheckMenuItem {
                                id: vinylItem

                                checkable: true
                                enabled: Mixxx.Application.vinylControlAvailable
                                text: qsTr("Vinyl control")

                                onTriggered: showVinylControl.value = checked ? 1 : 0
                            }
                            Skin.CheckMenuItem {
                                id: previewDeckItem

                                checkable: true
                                text: qsTr("Preview deck")

                                onTriggered: {
                                    if (!checked && previewDeckBar.item) {
                                        previewDeckBar.item.hide();
                                    }
                                    showPreviewDecks.value = checked ? 1 : 0;
                                }
                            }
                        }
                    }
                    Skin.Button {
                        id: showEffectUnitsButton

                        Accessible.checkable: true
                        Accessible.checked: show4EffectUnitsControl.value > 0
                        Accessible.name: qsTr("Show effect units 3 and 4")
                        activeColor: Theme.textOnAccent
                        checkable: true
                        text: qsTr("FX 3-4")
                        ToolTip.delay: 500
                        ToolTip.text: Accessible.name
                        ToolTip.visible: hovered

                        onClicked: show4EffectUnitsControl.value = checked ? 1 : 0

                        // checked has no declarative binding: AbstractButton
                        // overwrites it on every click, so mirror the control
                        // here instead and keep controller changes in sync.
                        Connections {
                            target: show4EffectUnitsControl

                            function onValueChanged() {
                                showEffectUnitsButton.checked = show4EffectUnitsControl.value > 0;
                            }
                        }
                        Component.onCompleted: showEffectUnitsButton.checked = show4EffectUnitsControl.value > 0

                        Skin.EffectUnitsWindow {
                            id: effectUnitsWindow
                        }
                    }
                    Skin.Button {
                        id: showPadFxButton

                        activeColor: Theme.textOnAccent
                        Layout.preferredWidth: 72
                        checkable: true
                        checked: padFxWindow.visible
                        text: qsTr("Pad FX")

                        Accessible.name: qsTr("Show Pad FX window")

                        ToolTip.delay: 500
                        ToolTip.text: qsTr("Per-stem performance pads for each deck")
                        ToolTip.visible: showPadFxButton.hovered

                        onClicked: {
                            if (padFxWindow.visible)
                                padFxWindow.close();
                            else
                                padFxWindow.show();
                        }

                        Skin.PadFxWindow {
                            id: padFxWindow

                            deckCount: root.show4decks ? 4 : 2
                        }
                    }
                    Skin.Button {
                        id: showControllerMappingButton

                        Accessible.name: qsTr("Open the MIDI controller mapping window")
                        Layout.preferredWidth: 56
                        activeColor: Theme.textOnAccent
                        text: qsTr("Map")

                        ToolTip.delay: 500
                        ToolTip.text: qsTr("Map controls on your MIDI controller")
                        ToolTip.visible: showControllerMappingButton.hovered

                        onClicked: {
                            controllerMappingWindow.show();
                            controllerMappingWindow.raise();
                            controllerMappingWindow.requestActivate();
                        }

                        ControllerMappingComponents.ControllerMappingWindow {
                            id: controllerMappingWindow

                            applicationWindow: root.applicationWindow
                        }
                    }
                    Skin.Button {
                        id: showDevToolsButton

                        activeColor: Theme.textOnAccent
                        Layout.preferredWidth: 84
                        checkable: true
                        checked: devToolsWindow.visible
                        text: qsTr("Develop")
                        visible: Mixxx.Application.developerMode

                        Accessible.name: qsTr("Show developer tools")

                        onClicked: {
                            if (devToolsWindow.visible)
                                devToolsWindow.close();
                            else
                                devToolsWindow.show();
                        }

                        DeveloperToolsWindow {
                            id: devToolsWindow

                            height: 480
                            width: 640
                        }
                    }
                    Skin.Button {
                        id: showPreferencesButton

                        activeColor: Theme.textOnAccent
                        icon.height: 16
                        icon.source: "images/gear.svg"
                        icon.width: 16
                        implicitWidth: implicitHeight

                        Accessible.name: qsTr("Preferences")

                        onClicked: Mixxx.PreferencesDialog.show()
                    }
                }
            }
            // Effects strip: the standard chains and the per-deck quick chains.
            Skin.TopBarEffects {
                id: topBarEffects

                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: mainToolbarRow.bottom
                anchors.bottom: parent.bottom
                anchors.leftMargin: Theme.space3
                anchors.rightMargin: Theme.space2
            }
            // Bottom edge of the whole top bar.
            Rectangle {
                anchors.bottom: parent.bottom
                color: Theme.border
                height: 1
                width: parent.width
                z: 2
            }
        }
        SplitView {
            id: splitView

            height: parent.height - y
            orientation: Qt.Vertical
            width: parent.width

            handle: Rectangle {
                id: handleDelegate

                property color handleColor: SplitHandle.pressed || SplitHandle.hovered ? Theme.libraryPanelSplitterHandleActive : Theme.borderStrong
                property int handleSize: SplitHandle.pressed || SplitHandle.hovered ? 6 : 3

                clip: true
                color: Theme.surface0
                implicitHeight: Theme.space1
                implicitWidth: Theme.space2

                containmentMask: Item {
                    height: Theme.space2
                    width: splitView.width
                    x: (handleDelegate.width - width) / 2
                }

                RowLayout {
                    anchors.centerIn: parent

                    Repeater {
                        model: 3

                        Rectangle {
                            color: handleColor
                            height: handleSize
                            radius: handleSize
                            width: handleSize
                        }
                    }
                }
            }

            Item {
                id: waveforms

                SplitView.fillHeight: !library.active && Mixxx.Config.paramixShowWaveforms
                SplitView.preferredHeight: library.active ? (Theme.shortWindow ? 96 : 160) : undefined
                visible: !root.maximizeLibrary && Mixxx.Config.paramixShowWaveforms

                // Only decks 1 and 2 get a waveform in the top strip. With four
                // decks enabled the strip keeps these two rows at full height
                // instead of splitting into four, so the stem lanes stay
                // readable and no vertical space is spent on decks 3 and 4
                // (they are still fully usable from their deck panes).
                Skin.WaveformDisplay {
                    id: deck1waveform

                    anchors.top: parent.top
                    group: "[Channel1]"
                    height: parent.height / 2
                    width: root.width
                }
                Skin.WaveformDisplay {
                    id: deck2waveform

                    anchors.bottom: parent.bottom
                    group: "[Channel2]"
                    height: parent.height / 2
                    width: root.width
                }
                Rectangle {
                    width: 125

                    gradient: Gradient {
                        orientation: Gradient.Horizontal

                        GradientStop {
                            color: Theme.surface0
                            position: 0
                        }
                        GradientStop {
                            color: 'transparent'
                            position: 1
                        }
                    }

                    anchors {
                        bottom: parent.bottom
                        left: parent.left
                        top: parent.top
                    }
                }
                Rectangle {
                    width: 125

                    gradient: Gradient {
                        orientation: Gradient.Horizontal

                        GradientStop {
                            color: 'transparent'
                            position: 0
                        }
                        GradientStop {
                            color: Theme.surface0
                            position: 1
                        }
                    }

                    anchors {
                        bottom: parent.bottom
                        right: parent.right
                        top: parent.top
                    }
                }
            }
            Item {
                SplitView.fillHeight: library.active || !Mixxx.Config.paramixShowWaveforms
                SplitView.maximumHeight: (library.active || !Mixxx.Config.paramixShowWaveforms)
                        ? undefined
                        : root.bandContentHeight + root.bandChromeHeight
                SplitView.minimumHeight: root.bandMinimumHeight

                Deck {
                    id: deck1

                    group: "[Channel1]"
                    height: root.maximizeLibrary ? 80 : root.show4decks ? root.bandContentHeight / 2 : root.bandContentHeight
                    minimized: root.maximizeLibrary
                    states: [
                        State {
                            when: root.maximizeLibrary

                            AnchorChanges {
                                anchors.right: parent.horizontalCenter
                                target: deck1
                            }
                        }
                    ]

                    onToggleFocus: {
                        root.focusedDeck = (root.focusedDeck === deck1) ? null : deck1;
                    }

                    anchors {
                        left: parent.left
                        right: mixer.left
                        top: parent.top
                    }
                }
                Mixer {
                    id: mixer

                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    groups: [deck1.group, deck2.group, deck3.group, deck4.group]
                    // Span the band so no gap opens under a mixer that is
                    // shorter than the decks beside it.
                    height: root.bandContentHeight
                    show4decks: root.show4decks
                    visible: !root.maximizeLibrary
                    // A focused deck at a narrow window takes the whole width with
                    // the mixer and the other deck hidden. Otherwise the decks are
                    // anchored to the mixer and their width follows the band.
                    states: [
                        State {
                            when: root.focusedDeck === deck1 && root.width < Theme.focusedDeckMinWindowWidth && !root.maximizeLibrary

                            AnchorChanges {
                                anchors.right: parent.right
                                target: deck1
                            }
                            PropertyChanges {
                                target: deck2
                                visible: false
                            }
                            PropertyChanges {
                                target: mixer
                                visible: false
                            }
                        },
                        State {
                            when: root.focusedDeck === deck2 && root.width < Theme.focusedDeckMinWindowWidth && !root.maximizeLibrary

                            AnchorChanges {
                                anchors.left: parent.left
                                target: deck2
                            }
                            PropertyChanges {
                                target: deck1
                                visible: false
                            }
                            PropertyChanges {
                                target: mixer
                                visible: false
                            }
                        },
                        State {
                            when: root.maximizeLibrary

                            AnchorChanges {
                                anchors.horizontalCenter: parent.horizontalCenter
                                target: mixer
                            }
                        }
                    ]
                }
                Deck {
                    id: deck2

                    group: "[Channel2]"
                    height: root.maximizeLibrary ? 80 : root.show4decks ? root.bandContentHeight / 2 : root.bandContentHeight
                    minimized: root.maximizeLibrary
                    states: [
                        State {
                            when: root.maximizeLibrary

                            AnchorChanges {
                                anchors.left: parent.horizontalCenter
                                target: deck2
                            }
                        }
                    ]

                    onToggleFocus: {
                        root.focusedDeck = (root.focusedDeck === deck2) ? null : deck2;
                    }

                    anchors {
                        left: mixer.right
                        right: parent.right
                        top: parent.top
                    }
                }
                Loader {
                    id: deck3

                    readonly property string group: "[Channel3]"

                    active: root.show4decks
                    height: active ? (root.maximizeLibrary ? 80 : root.bandContentHeight / 2) : 0
                    sourceComponent: Component {
                        Deck {
                            anchors.fill: parent
                            group: deck3.group
                            minimized: root.maximizeLibrary
                        }
                    }
                    states: [
                        State {
                            when: root.maximizeLibrary

                            AnchorChanges {
                                anchors.right: parent.horizontalCenter
                                target: deck3
                            }
                        }
                    ]

                    anchors {
                        left: parent.left
                        right: mixer.left
                        top: deck1.bottom
                    }
                }
                Loader {
                    id: deck4

                    readonly property string group: "[Channel4]"

                    active: root.show4decks
                    height: active ? (root.maximizeLibrary ? 80 : root.bandContentHeight / 2) : 0
                    sourceComponent: Component {
                        Deck {
                            anchors.fill: parent
                            group: deck4.group
                            minimized: root.maximizeLibrary
                        }
                    }
                    states: [
                        State {
                            when: root.maximizeLibrary

                            AnchorChanges {
                                anchors.left: parent.horizontalCenter
                                target: deck4
                            }
                        }
                    ]

                    anchors {
                        left: mixer.right
                        right: parent.right
                        top: deck2.bottom
                    }
                }

                MixerComponents.OutputBar {
                    id: outputBar

                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    // Below whichever is taller. Anchoring to the mixer alone
                    // drew this bar over the decks' bottom rows whenever the
                    // mixer came out shorter than Theme.deckMinHeight.
                    anchors.topMargin: Math.max(mixer.height, deck1.height + deck3.height)
                    height: visible ? implicitHeight : 0
                    show4decks: root.show4decks
                    visible: !root.maximizeLibrary
                }
                Loader {
                    id: previewDeckBar

                    // Kept active while hidden so preview playback started from
                    // the keyboard can reveal the bar instead of playing unseen.
                    active: true
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: outputBar.bottom
                    height: visible ? implicitHeight : 0
                    visible: (showPreviewDecks.value > 0 ||
                              (previewDeckBar.item && previewDeckBar.item.revealed) ||
                              previewDeckPlaying.value > 0) &&
                             !root.maximizeLibrary
                    sourceComponent: Component {
                        MixerComponents.PreviewDeckBar {
                            anchors.fill: parent
                        }
                    }
                }
                Loader {
                    id: inputRack

                    active: root.showMicrophoneInputs &&
                            (numMicrophones.value > 0 || numAuxiliaries.value > 0) &&
                            !root.maximizeLibrary
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: previewDeckBar.bottom
                    height: active ? implicitHeight : 0
                    visible: active
                    sourceComponent: Component {
                        MixerComponents.InputRack {
                            auxiliaryCount: numAuxiliaries.value
                            // Collapse to the header only when the window is
                            // shorter than the design minimum; the old literal
                            // 1000 collapsed it on every laptop display.
                            defaultCollapsed: library.active && root.height < Theme.minWindowHeight
                            microphoneCount: numMicrophones.value
                        }
                    }
                }
                Loader {
                    id: samplers

                    active: root.showSamplers && !root.maximizeLibrary
                    height: active ? implicitHeight : 0
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: inputRack.bottom
                    sourceComponent: Component {
                        Skin.SamplerRow {
                        }
                    }
                }
                Loader {
                    id: library

                    active: (root.maximizeLibrary || root.height - mixer.height >= 400) && Mixxx.Config.paramixShowLibrary
                    width: parent.width

                    sourceComponent: Component {
                        Skin.Library {
                            anchors.fill: parent
                        }
                    }
                    states: [
                        State {
                            when: root.maximizeLibrary && root.show4decks

                            AnchorChanges {
                                anchors.top: deck4.bottom
                                target: library
                            }
                        },
                        State {
                            when: root.maximizeLibrary && !root.show4decks

                            AnchorChanges {
                                anchors.top: deck1.bottom
                                target: library
                            }
                        },
                        State {
                            when: !root.maximizeLibrary && root.height - mixer.height < 400

                            PropertyChanges {
                                target: library
                                visible: false
                            }
                        }
                    ]

                    anchors {
                        bottom: parent.bottom
                        top: samplers.bottom
                    }
                }
            }
        }
    }
}
