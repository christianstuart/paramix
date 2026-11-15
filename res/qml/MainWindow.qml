import "." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import QtQuick.Shapes
import Qt5Compat.GraphicalEffects
import "Mixer" as MixerComponents
import "Theme"

Item {
    id: root

    required property ApplicationWindow applicationWindow
    property alias menuBar: nativeApplicationMenuLoader.item

    property alias editDeck: editDeckButton.checked
    property var focusedDeck: null
    property alias maximizeLibrary: maximizeLibraryButton.checked
    readonly property int numDecks: 4
    readonly property int numPreviewDecks: 1
    readonly property int numSamplers: 16
    readonly property bool show4decks: show4DecksButton.checked && show4DecksButton.visible
    property alias showEffects: showEffectsButton.checked
    property alias showSamplers: showSamplersButton.checked
    readonly property bool showMicrophoneInputs: showMicrophonesControl.value > 0
    readonly property int waveformOverviewTypeRgb: 2

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
        group: "[App]"
        key: "num_preview_decks"

        onInitializedChanged: {
            value = root.numPreviewDecks;
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
        id: showMicrophonesControl

        group: "[Skin]"
        key: "show_microphones"

        onValueChanged: {
            if (showAuxButton) {
                showAuxButton.checked = showMicrophonesControl.value > 0;
            }
        }
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
            height: Theme.toolbarHeight
            width: parent.width

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

                // Brand plate: signal bar, wordmark, deck status dots.
                RowLayout {
                    spacing: Theme.space2

                    Rectangle {
                        color: Theme.accent
                        implicitHeight: 22
                        implicitWidth: 3
                        radius: 1
                    }
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
                Rectangle {
                    Layout.alignment: Qt.AlignVCenter
                    color: Theme.border
                    implicitHeight: 20
                    implicitWidth: 1
                }
                Skin.Button {
                    id: show4DecksButton

                    activeColor: Theme.textOnAccent
                    Layout.preferredWidth: 84
                    checkable: true
                    text: qsTr("4 Decks")
                    visible: root.height > 515

                    Accessible.name: qsTr("Show four decks")
                }
                Skin.Button {
                    id: maximizeLibraryButton

                    activeColor: Theme.textOnAccent
                    Layout.preferredWidth: 84
                    checkable: true
                    text: qsTr("Library")

                    Accessible.name: qsTr("Maximize library")

                    onCheckedChanged: () => {
                        showMaximizedLibrary.value = this.checked;
                    }

                    Mixxx.ControlProxy {
                        id: showMaximizedLibrary

                        group: "[Skin]"
                        key: "show_maximized_library"

                        onValueChanged: () => {
                            maximizeLibraryButton.checked = this.value;
                        }
                    }
                }
                Skin.Button {
                    id: showEffectsButton

                    activeColor: Theme.textOnAccent
                    Layout.preferredWidth: 84
                    checkable: true
                    text: qsTr("Effects")

                    Accessible.name: qsTr("Show effects")
                }
                Skin.Button {
                    id: showAuxButton

                    activeColor: Theme.textOnAccent
                    Layout.preferredWidth: 84
                    checkable: true
                    text: qsTr("Aux")

                    Accessible.name: qsTr("Show microphone and auxiliary inputs")

                    Component.onCompleted: checked = root.showMicrophoneInputs
                    onCheckedChanged: showMicrophonesControl.value = showAuxButton.checked
                }
                Skin.Button {
                    id: showSamplersButton

                    activeColor: Theme.textOnAccent
                    Layout.preferredWidth: 84
                    checkable: true
                    text: qsTr("Sampler")

                    Accessible.name: qsTr("Show samplers")
                }
                Item {
                    Layout.fillWidth: true
                }
                Skin.Button {
                    id: editDeckButton

                    activeColor: Theme.textOnAccent
                    Layout.preferredWidth: 84
                    checkable: true
                    text: qsTr("Edit")

                    Accessible.name: qsTr("Edit deck layout")
                }
                Skin.Button {
                    id: showDevToolsButton

                    activeColor: Theme.textOnAccent
                    Layout.preferredWidth: 84
                    checkable: true
                    checked: devToolsWindow.visible
                    text: qsTr("Develop")

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
                Skin.Button {
                    id: customizeInterfaceButton

                    activeColor: Theme.textOnAccent
                    icon.height: 16
                    icon.source: "images/sliders.svg"
                    icon.width: 16
                    implicitWidth: implicitHeight

                    Accessible.name: qsTr("Customize interface")

                    onClicked: customizePopup.open()
                }
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
                SplitView.preferredHeight: library.active ? 120 : undefined
                visible: !root.maximizeLibrary && Mixxx.Config.paramixShowWaveforms

                FadeBehavior on visible {
                    fadeTarget: waveforms
                }

                Loader {
                    id: deck3waveform

                    readonly property string group: "[Channel3]"

                    active: root.show4decks
                    anchors.top: parent.top
                    height: parent.height / 4
                    width: root.width

                    sourceComponent: Component {
                        Skin.WaveformDisplay {
                            group: deck3waveform.group

                            FadeBehavior on visible {
                                fadeTarget: deck3waveform
                            }
                        }
                    }
                }
                Skin.WaveformDisplay {
                    id: deck1waveform

                    anchors.top: root.show4decks ? deck3waveform.bottom : parent.top
                    group: "[Channel1]"
                    height: parent.height / (root.show4decks ? 4 : 2)
                    width: root.width
                }
                Skin.WaveformDisplay {
                    id: deck2waveform

                    anchors.bottom: root.show4decks ? deck4waveform.top : parent.bottom
                    group: "[Channel2]"
                    height: parent.height / (root.show4decks ? 4 : 2)
                    width: root.width
                }
                Loader {
                    id: deck4waveform

                    readonly property string group: "[Channel4]"

                    active: root.show4decks
                    anchors.bottom: parent.bottom
                    height: parent.height / 4
                    width: root.width

                    sourceComponent: Component {
                        Skin.WaveformDisplay {
                            group: deck4waveform.group

                            FadeBehavior on visible {
                                fadeTarget: deck4waveform
                            }
                        }
                    }
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
                        : mixer.height
                SplitView.minimumHeight: mixer.height

                Deck {
                    id: deck1

                    editMode: root.editDeck
                    group: "[Channel1]"
                    height: root.maximizeLibrary ? 80 : root.show4decks ? mixer.height / 2 : mixer.height
                    minimized: root.maximizeLibrary

                    Behavior on height {
                        SpringAnimation {
                            id: deck1HeightAnimation

                            damping: 0.35
                            duration: Theme.motionSpringDuration
                            spring: 2.4
                        }
                    }
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
                    show4decks: root.show4decks
                    visible: !root.maximizeLibrary

                    Behavior on height {
                        SpringAnimation {
                            id: mixerHeightAnimation

                            damping: 0.35
                            duration: Theme.motionSpringDuration
                            spring: 2.4
                        }
                    }
                    states: [
                        State {
                            when: root.focusedDeck === deck1 && root.width < 1400 && !root.maximizeLibrary

                            AnchorChanges {
                                anchors.horizontalCenter: parent.right
                                target: mixer
                            }
                            PropertyChanges {
                                target: deck1
                                width: root.width - (mixer.width / 2)
                            }
                        },
                        State {
                            when: root.focusedDeck === deck2 && root.width < 1400 && !root.maximizeLibrary

                            AnchorChanges {
                                anchors.horizontalCenter: parent.left
                                target: mixer
                            }
                            PropertyChanges {
                                target: deck2
                                width: root.width - (mixer.width / 2)
                            }
                        },
                        State {
                            when: (!root.focusedDeck || root.width > 1400) && !root.maximizeLibrary

                            AnchorChanges {
                                anchors.horizontalCenter: parent.horizontalCenter
                                target: mixer
                            }
                            PropertyChanges {
                                target: deck1
                                width: (root.width - mixer.width) / 2
                            }
                            PropertyChanges {
                                target: deck2
                                width: (root.width - mixer.width) / 2
                            }
                        },
                        State {
                            when: root.maximizeLibrary

                            AnchorChanges {
                                anchors.horizontalCenter: parent.horizontalCenter
                                target: mixer
                            }
                            PropertyChanges {
                                target: deck1
                                width: root.width / 2
                            }
                            PropertyChanges {
                                target: deck2
                                width: root.width / 2
                            }
                        }
                    ]
                    transitions: Transition {
                        AnchorAnimation {
                            duration: Theme.motionNormal
                        }
                    }
                    FadeBehavior on visible {
                        fadeTarget: mixer
                    }
                }
                Deck {
                    id: deck2

                    editMode: root.editDeck
                    group: "[Channel2]"
                    height: root.maximizeLibrary ? 80 : root.show4decks ? mixer.height / 2 : mixer.height
                    minimized: root.maximizeLibrary

                    Behavior on height {
                        SpringAnimation {
                            id: deck2HeightAnimation

                            damping: 0.35
                            duration: Theme.motionSpringDuration
                            spring: 2.4
                        }
                    }
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
                    height: active ? (root.maximizeLibrary ? 80 : mixer.height / 2) : 0

                    Behavior on height {
                        SpringAnimation {
                            id: deck3HeightAnimation

                            damping: 0.35
                            duration: Theme.motionSpringDuration
                            spring: 2.4
                        }
                    }
                    sourceComponent: Component {
                        Deck {
                            anchors.bottom: parent.bottom
                            anchors.left: parent.left
                            editMode: root.editDeck
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
                    height: active ? (root.maximizeLibrary ? 80 : mixer.height / 2) : 0

                    Behavior on height {
                        SpringAnimation {
                            id: deck4HeightAnimation

                            damping: 0.35
                            duration: Theme.motionSpringDuration
                            spring: 2.4
                        }
                    }
                    sourceComponent: Component {
                        Deck {
                            anchors.bottom: parent.bottom
                            anchors.right: parent.right
                            editMode: root.editDeck
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

                Loader {
                    id: inputRack

                    active: root.showMicrophoneInputs &&
                            (numMicrophones.value > 0 || numAuxiliaries.value > 0) &&
                            !root.maximizeLibrary
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: mixer.bottom
                    visible: active
                    sourceComponent: Component {
                        MixerComponents.InputRack {
                            auxiliaryCount: numAuxiliaries.value
                            defaultCollapsed: library.active && root.height < 1000
                            microphoneCount: numMicrophones.value
                        }
                    }
                }
                Loader {
                    id: effects

                    active: root.showEffects
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: inputRack.bottom
                    sourceComponent: Component {
                        Skin.EffectRow {
                        }
                    }
                }
                Loader {
                    id: samplers

                    active: root.showSamplers
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: effects.bottom
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

    // Global job strip (stem separation, analysis). It overlays the top of the
    // content area instead of taking part in the Column above, so a running
    // job never reflows the decks or the library. It is invisible and zero
    // height while idle.
    Skin.TaskStatusStrip {
        id: taskStatusStrip

        anchors.left: content.left
        anchors.right: content.right
        anchors.top: content.top
        anchors.topMargin: toolbar.height
        z: 2
    }
    Skin.ParamixCustomize {
        id: customizePopup
    }
}
