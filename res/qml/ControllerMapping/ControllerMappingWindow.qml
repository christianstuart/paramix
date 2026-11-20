import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts
import QtQuick.Window 2.15
import Mixxx 1.0 as Mixxx
import ".." as Skin
import "../Theme"

/// Controller-based MIDI mapping window (F10, PR 2).
///
/// Hardware-first: choose a MIDI device, move a physical control, then pick the
/// Paramix control it should drive. All editing goes through
/// `Mixxx.ControllerLearning`; saving writes a normal user mapping file, and a
/// bundled mapping is copied instead of modified.
///
/// Learn-by-click delegates to `ControllerMappingLearnOverlay`, which reparents
/// itself into the main window's content item while learn mode is on.
Window {
    id: root

    /// The main `ApplicationWindow`, passed by MainWindow for learn-by-click.
    property var applicationWindow: null
    readonly property var controlEntries: indexLoader.item && indexLoader.item.ready ? indexLoader.item.entries : []
    readonly property bool hasMessage: Mixxx.ControllerLearning.lastMessage && Mixxx.ControllerLearning.lastMessage.message !== undefined
    readonly property var existingMapping: root.hasMessage ? Mixxx.ControllerLearning.existingMappingFor(Mixxx.ControllerLearning.lastMessage) : ({})
    readonly property string detectedType: root.hasMessage ? Mixxx.ControllerLearning.lastMessage.type : ""
    property bool indexRequested: false
    property bool learnMode: false
    readonly property string mappingLabel: {
        if (!root.selectedDevice)
            return qsTr("No device selected");
        if (Mixxx.ControllerLearning.mappingName.length > 0)
            return Mixxx.ControllerLearning.mappingName;
        if (root.selectedDevice.mapping)
            return root.selectedDevice.mapping.name;
        return qsTr("New mapping");
    }
    /// 0: no mapping yet, 1: bundled mapping, 2: user mapping.
    readonly property int mappingKind: {
        const mapping = root.selectedDevice ? root.selectedDevice.mapping : null;
        if (!mapping)
            return 0;
        if (typeof mapping.isUserMapping !== "function")
            return 0;
        return mapping.isUserMapping(Mixxx.Config) ? 2 : 1;
    }
    property var midiDevices: []
    /// Set when anything in the working mapping changed since the last save.
    property bool modified: false
    property string overrideType: "auto"
    /// Order matches the override ComboBox model.
    readonly property var overrideTypes: ["auto", "button", "knob", "relative"]
    property string savedPath: ""
    property var selectedDevice: null
    readonly property string statusLine: Mixxx.ControllerLearning.lastError.length > 0 ? Mixxx.ControllerLearning.lastError : root.statusText
    property string statusText: ""

    color: Theme.surface0
    height: 720
    minimumHeight: 600
    minimumWidth: 900
    title: qsTr("Controller mapping")
    visible: false
    width: 1120

    onClosing: {
        root.learnMode = false;
        Mixxx.ControllerLearning.stop();
    }
    onVisibleChanged: {
        if (!visible) {
            root.learnMode = false;
            return;
        }
        root.indexRequested = true;
        root.syncDevices();
        if (root.selectedDevice && !Mixxx.ControllerLearning.learning)
            root.startLearning(root.selectedDevice);
        else
            root.maybeAutoSelect();
    }

    Connections {
        function onDeviceListChanged() {
            root.syncDevices();
            if (root.visible)
                root.maybeAutoSelect();
        }

        // The QML skin loads before CoreServices registers the ControllerManager.
        // Do not touch the singleton until the window is opened.
        target: root.visible ? Mixxx.ControllerManager : null
    }
    Connections {
        function onMappingChanged() {
            root.modified = true;
        }

        target: Mixxx.ControllerLearning
    }

    Loader {
        id: indexLoader

        active: root.indexRequested
        source: "ControllerMappingControlIndex.qml"
    }
    ControllerMappingLearnOverlay {
        id: learnOverlay

        active: root.learnMode && root.visible
        controlEntries: root.controlEntries
        hostItem: root.applicationWindow ? root.applicationWindow.contentItem : null
        instruction: root.hasMessage ? qsTr("Click the control that should be driven by %1").arg(Mixxx.ControllerLearning.lastMessage.message) : qsTr("Click the control that should be driven by this hardware control")

        onCancelled: {
            root.learnMode = false;
            root.requestActivate();
        }
        onControlPicked: function (group, key) {
            root.assignControl(group, key);
            root.requestActivate();
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.space3
        spacing: Theme.space2

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space2

            Text {
                Accessible.role: Accessible.StaticText
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeLg
                font.weight: Theme.fontWeightDemiBold
                text: qsTr("Controller mapping")
                textFormat: Text.PlainText
            }
            Item {
                Layout.fillWidth: true
            }
            Skin.Button {
                id: closeKey

                Accessible.name: qsTr("Close the controller mapping window")
                text: qsTr("Close")

                ToolTip.delay: 500
                ToolTip.text: qsTr("Learning stops when the window closes")
                ToolTip.visible: closeKey.hovered

                onClicked: root.close()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space2

            Skin.ComboBox {
                id: deviceCombo

                Accessible.name: qsTr("MIDI device")
                Layout.preferredWidth: 280
                currentIndex: -1
                displayText: currentIndex >= 0 ? textAt(currentIndex) : qsTr("Choose a device")
                model: root.midiDevices.map(function (device) {
                    return device.name;
                })
                visible: root.midiDevices.length > 1

                ToolTip.delay: 500
                ToolTip.text: qsTr("Switch the device being mapped")
                ToolTip.visible: hovered

                onActivated: root.startLearning(root.midiDevices[currentIndex])
            }
            Text {
                Layout.fillWidth: true
                Accessible.role: Accessible.StaticText
                color: Theme.textSecondary
                elide: Text.ElideRight
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeSm
                text: {
                    if (root.midiDevices.length === 0)
                        return qsTr("No MIDI controller connected");
                    if (root.midiDevices.length === 1)
                        return qsTr("Device: %1").arg(root.midiDevices[0].name);
                    return root.selectedDevice ? qsTr("Device: %1").arg(root.selectedDevice.name) : qsTr("Choose a device to start");
                }
                textFormat: Text.PlainText
            }
            Text {
                Accessible.role: Accessible.StaticText
                color: Theme.textMuted
                elide: Text.ElideRight
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeSm
                text: qsTr("Mapping: %1").arg(root.mappingLabel)
                textFormat: Text.PlainText
            }
        }
        Text {
            Layout.fillWidth: true
            Accessible.role: Accessible.StaticText
            color: root.mappingKind === 1 ? Theme.warning : Theme.textMuted
            elide: Text.ElideRight
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeXs
            text: {
                if (root.mappingKind === 1)
                    return qsTr("Bundled mapping — Save writes a copy to your user mappings and leaves the bundled file untouched.");
                if (root.mappingKind === 2)
                    return qsTr("User mapping — Save overwrites this file.");
                return qsTr("Save creates a user mapping file.");
            }
            textFormat: Text.PlainText
        }

        RowLayout {
            Layout.fillHeight: true
            Layout.fillWidth: true
            spacing: Theme.space3

            ColumnLayout {
                Layout.fillHeight: true
                Layout.fillWidth: true
                spacing: Theme.space2

                ControllerMappingMessageCard {
                    Layout.fillWidth: true
                    existing: root.existingMapping
                    learning: Mixxx.ControllerLearning.learning
                    message: Mixxx.ControllerLearning.lastMessage
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.space2

                    Text {
                        Accessible.role: Accessible.StaticText
                        color: Theme.textSecondary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeXs
                        text: qsTr("DETECTED")
                        textFormat: Text.PlainText
                    }
                    Text {
                        Accessible.role: Accessible.StaticText
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeSm
                        font.weight: Theme.fontWeightMedium
                        text: root.hasMessage ? root.typeLabel(root.detectedType) : qsTr("Waiting for a message")
                        textFormat: Text.PlainText
                    }
                    Text {
                        Accessible.role: Accessible.StaticText
                        Layout.leftMargin: Theme.space3
                        color: Theme.textSecondary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeXs
                        text: qsTr("OVERRIDE")
                        textFormat: Text.PlainText
                    }
                    Skin.ComboBox {
                        id: overrideCombo

                        Accessible.description: qsTr("Overrides how the detected message is interpreted")
                        Accessible.name: qsTr("Detected option override")
                        Layout.preferredWidth: 190
                        model: [qsTr("Auto-detect"), qsTr("Button"), qsTr("Knob (absolute)"), qsTr("Relative encoder")]

                        ToolTip.delay: 500
                        ToolTip.text: qsTr("A manual override replaces the detected button/knob/relative flags")
                        ToolTip.visible: hovered

                        onActivated: root.overrideType = root.overrideTypes[currentIndex]
                    }
                    Item {
                        Layout.fillWidth: true
                    }
                    Skin.Button {
                        id: learnByClickKey

                        Accessible.description: qsTr("Highlights the main window and maps the next control you click")
                        Accessible.name: qsTr("Learn by clicking a control")
                        enabled: root.hasMessage && root.applicationWindow !== null && Mixxx.ControllerLearning.learning
                        text: qsTr("Click a control")

                        ToolTip.delay: 500
                        ToolTip.text: qsTr("Close this window's controls and click a knob, fader or button in the main window")
                        ToolTip.visible: learnByClickKey.hovered

                        onClicked: {
                            root.learnMode = !root.learnMode;
                            if (root.learnMode)
                                root.activateMainWindow();
                        }
                    }
                }
                ControllerMappingControlList {
                    Layout.fillHeight: true
                    Layout.fillWidth: true
                    entries: root.controlEntries
                    ready: indexLoader.item ? indexLoader.item.ready : false

                    onControlChosen: function (group, key) {
                        root.assignControl(group, key);
                    }
                }
            }
            ColumnLayout {
                Layout.fillHeight: true
                Layout.preferredWidth: 360
                spacing: Theme.space2

                ControllerMappingPendingList {
                    id: pendingList

                    Layout.fillHeight: true
                    Layout.fillWidth: true
                    learning: Mixxx.ControllerLearning.learning
                }
                Rectangle {
                    Layout.fillWidth: true
                    border.color: Theme.border
                    border.width: 1
                    color: Theme.surface1
                    implicitHeight: saveLayout.implicitHeight + 2 * Theme.space3
                    radius: Theme.radiusMd

                    ColumnLayout {
                        id: saveLayout

                        anchors.fill: parent
                        anchors.margins: Theme.space3
                        spacing: Theme.space2

                        Text {
                            Accessible.role: Accessible.StaticText
                            color: Theme.textPrimary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeSm
                            font.weight: Theme.fontWeightDemiBold
                            text: qsTr("Save mapping")
                            textFormat: Text.PlainText
                        }
                        Text {
                            Layout.fillWidth: true
                            Accessible.role: Accessible.StaticText
                            color: Theme.textMuted
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeXs
                            text: qsTr("Unsaved mappings are lost when the window closes.")
                            textFormat: Text.PlainText
                            wrapMode: Text.WordWrap
                        }
                        Skin.TextField {
                            id: mappingNameField

                            Accessible.description: qsTr("Name of the mapping file to write")
                            Accessible.name: qsTr("Mapping name")
                            Layout.fillWidth: true
                            placeholderText: qsTr("Mapping name")
                            ToolTip.delay: 500
                            ToolTip.text: qsTr("Used for the file name of the user mapping")
                            ToolTip.visible: hovered
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.space2

                            Skin.Button {
                                id: saveKey

                                Accessible.name: qsTr("Save the mapping to the user mapping directory")
                                enabled: root.modified && root.selectedDevice !== null
                                text: qsTr("Save")

                                ToolTip.delay: 500
                                ToolTip.text: qsTr("Write a user mapping file and reopen the device with it")
                                ToolTip.visible: saveKey.hovered

                                onClicked: root.saveMapping()
                            }
                            Skin.Button {
                                id: continueKey

                                Accessible.name: qsTr("Continue mapping after saving")
                                text: qsTr("Continue")

                                ToolTip.delay: 500
                                ToolTip.text: qsTr("Start a new learning session on the same device")
                                ToolTip.visible: continueKey.hovered
                                visible: root.selectedDevice !== null && !Mixxx.ControllerLearning.learning

                                onClicked: root.startLearning(root.selectedDevice)
                            }
                            Item {
                                Layout.fillWidth: true
                            }
                        }
                        Text {
                            Layout.fillWidth: true
                            Accessible.role: Accessible.StaticText
                            color: Theme.positive
                            elide: Text.ElideMiddle
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeXs
                            text: root.savedPath
                            textFormat: Text.PlainText
                            visible: root.savedPath.length > 0
                            wrapMode: Text.WrapAnywhere
                        }
                    }
                }
            }
        }
        Text {
            Layout.fillWidth: true
            Accessible.role: Accessible.StaticText
            color: Mixxx.ControllerLearning.lastError.length > 0 ? Theme.danger : Theme.textMuted
            elide: Text.ElideRight
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeXs
            text: root.statusLine
            textFormat: Text.PlainText
            visible: root.statusLine.length > 0
        }
    }

    function activateMainWindow() {
        if (!root.applicationWindow)
            return;
        // The mapping window is a separate top-level window. Learn-by-click
        // only works while the main window is on top and receives the press.
        root.applicationWindow.raise();
        root.applicationWindow.requestActivate();
    }

    function assignControl(group, key) {
        if (!root.hasMessage) {
            root.statusText = qsTr("Move a control on the controller first.");
            return;
        }
        if (!Mixxx.ControllerLearning.learning) {
            root.statusText = qsTr("Learning is stopped. Continue the session first.");
            return;
        }
        if (Mixxx.ControllerLearning.assign(group, key, root.assignmentOptions())) {
            root.learnMode = false;
            root.statusText = qsTr("Mapped %1,%2").arg(group).arg(key);
        } else {
            root.statusText = Mixxx.ControllerLearning.lastError;
        }
    }

    function assignmentOptions() {
        // MidiOption values from src/controllers/midi/midimessage.h. Only the
        // modifier flags survive an override; the type flags are replaced.
        const optionInvert = 0x0001;
        const optionButton = 0x0020;
        const optionSelectKnob = 0x0200;
        const optionSoftTakeover = 0x0400;

        if (root.overrideType === "auto")
            return ({});

        const suggestions = Mixxx.ControllerLearning.suggestedMappings;
        let mask = 0;
        if (suggestions.length > 0)
            mask = suggestions[0].options & (optionInvert | optionSoftTakeover);
        if (root.overrideType === "button")
            mask = mask | optionButton;
        else if (root.overrideType === "relative")
            mask = mask | optionSelectKnob;
        // "knob" is absolute: no button, switch, relative or 14-bit flags.
        return {
            "options": mask
        };
    }

    function maybeAutoSelect() {
        if (!root.visible || root.selectedDevice !== null || Mixxx.ControllerLearning.learning)
            return;
        if (root.midiDevices.length !== 1)
            return;
        root.startLearning(root.midiDevices[0]);
    }

    function saveMapping() {
        const path = Mixxx.ControllerLearning.save(mappingNameField.text);
        if (path.length > 0) {
            root.savedPath = path;
            root.statusText = qsTr("Saved to %1").arg(path);
            root.modified = false;
        } else {
            root.statusText = Mixxx.ControllerLearning.lastError;
        }
    }

    function startLearning(device) {
        if (!device)
            return;
        if (root.sameDevice(root.selectedDevice, device) && Mixxx.ControllerLearning.learning)
            return;
        root.learnMode = false;
        root.overrideType = "auto";
        root.savedPath = "";
        root.statusText = "";
        if (!Mixxx.ControllerLearning.start(device)) {
            root.selectedDevice = null;
            root.statusText = Mixxx.ControllerLearning.lastError;
            return;
        }
        root.selectedDevice = device;
        root.modified = false;
        mappingNameField.text = Mixxx.ControllerLearning.mappingName.length > 0 ? Mixxx.ControllerLearning.mappingName : device.name;
        overrideCombo.currentIndex = 0;
    }

    function sameDevice(left, right) {
        if (!left || !right)
            return false;
        if (left === right)
            return true;
        // Device proxies can be recreated when the device list refreshes, so
        // fall back to the stable identity fields.
        return left.name === right.name && left.vendor === right.vendor && left.product === right.product && left.serialNumber === right.serialNumber;
    }

    function syncDevices() {
        const found = [];
        const collect = function (list) {
            for (let i = 0; i < list.length; ++i) {
                const device = list[i];
                if (device && device.type === Mixxx.ControllerDevice.MIDI)
                    found.push(device);
            }
        };
        collect(Mixxx.ControllerManager.knownDevices);
        collect(Mixxx.ControllerManager.unknownDevices);
        root.midiDevices = found;
        if (!root.selectedDevice)
            return;

        let current = null;
        for (let i = 0; i < found.length; ++i) {
            if (root.sameDevice(found[i], root.selectedDevice)) {
                current = found[i];
                break;
            }
        }
        if (current) {
            // Keep the backend pointed at the live proxy.
            root.selectedDevice = current;
            return;
        }
        Mixxx.ControllerLearning.stop();
        root.selectedDevice = null;
        root.statusText = qsTr("The mapped device was disconnected.");
    }

    function typeLabel(type) {
        switch (type) {
        case "button":
            return qsTr("Button");
        case "knob":
            return qsTr("Knob (absolute)");
        case "relative":
            return qsTr("Relative encoder");
        case "fourteenBit":
            return qsTr("14-bit");
        default:
            return qsTr("Unknown");
        }
    }
}
