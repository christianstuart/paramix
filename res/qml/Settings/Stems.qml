import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Mixxx 1.0 as Mixxx
import ".." as Skin
import "." as Setting
import "../Theme"

Category {
    id: root

    property bool dirty: false
    property string modelPath: ""
    property string outputDirectory: ""
    readonly property list<string> fileFormats: ["flac", "alac", "aac"]
    readonly property bool aacSelected: root.fileFormats[formatInput.currentIndex] === "aac"

    label: qsTr("Stems")

    function load() {
        const formatIndex = root.fileFormats.indexOf(Mixxx.Config.stemFileFormat);
        formatInput.currentIndex = formatIndex >= 0 ? formatIndex : 0;
        bitrateInput.value = Mixxx.Config.stemAacBitrate;
        root.modelPath = Mixxx.Config.stemModelPath;
        root.outputDirectory = Mixxx.Config.stemOutputDirectory;
        gpuAccelerationInput.selected = Mixxx.Config.stemGpuAcceleration ? "on" : "off";
        root.dirty = false;
    }
    function markDirty() {
        if (!root.dirty) {
            root.dirty = true;
        }
    }
    function reset() {
        formatInput.currentIndex = 0;
        bitrateInput.value = 256;
        root.modelPath = "";
        root.outputDirectory = "";
        gpuAccelerationInput.selected = Mixxx.Config.stemGpuAccelerationDefault ? "on" : "off";
        root.dirty = true;
    }
    function save() {
        Mixxx.Config.stemFileFormat = root.fileFormats[formatInput.currentIndex];
        Mixxx.Config.stemAacBitrate = bitrateInput.value;
        Mixxx.Config.stemModelPath = root.modelPath;
        Mixxx.Config.stemOutputDirectory = root.outputDirectory;
        Mixxx.Config.stemGpuAcceleration = gpuAccelerationInput.selected === "on";
        root.load();
    }

    Component.onCompleted: root.load()

    ScrollView {
        anchors.bottom: buttonActions.top
        anchors.bottomMargin: 18
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        clip: true

        ColumnLayout {
            anchors.left: parent.left
            anchors.margins: 14
            anchors.right: parent.right
            anchors.top: parent.top
            spacing: 10

            Text {
                Layout.fillWidth: true
                color: Theme.textPrimary
                font.pixelSize: Theme.fontSizeLg
                font.weight: Theme.fontWeightDemiBold
                text: qsTr("Stem export")
            }
            Text {
                Layout.fillWidth: true
                color: Theme.textSecondary
                font.pixelSize: Theme.fontSizeMd
                text: qsTr("Choose how separated stems are written to disk. The native stem export reads these settings.")
                wrapMode: Text.WordWrap
            }
            RowLayout {
                Layout.fillWidth: true

                StemsSettingParameter {
                    label: qsTr("Output format")
                }
                Skin.ComboBox {
                    id: formatInput

                    Accessible.name: qsTr("Output format")
                    Accessible.role: Accessible.ComboBox
                    Layout.preferredWidth: 200
                    model: [qsTr("FLAC (lossless)"), qsTr("ALAC (lossless)"), qsTr("AAC (lossy)")]

                    onCurrentIndexChanged: root.markDirty()
                }
            }
            Text {
                Layout.fillWidth: true
                color: Theme.textMuted
                font.pixelSize: Theme.fontSizeMd
                text: qsTr("FLAC and ALAC keep the separated stems lossless. AAC saves disk space but permanently discards detail.")
                wrapMode: Text.WordWrap
            }
            RowLayout {
                Layout.fillWidth: true

                StemsSettingParameter {
                    label: qsTr("AAC bitrate")
                }
                Setting.SpinBox {
                    id: bitrateInput

                    Accessible.name: qsTr("AAC bitrate")
                    Accessible.role: Accessible.SpinBox
                    enabled: root.aacSelected
                    Layout.preferredWidth: 160
                    max: 512
                    min: 32
                    opacity: enabled ? 1.0 : 0.4
                    precision: 0
                    suffix: qsTr(" kbps")
                    value: 256

                    onValueChanged: root.markDirty()
                }
                Setting.InfoBubble {
                    id: bitrateInfo

                    Accessible.name: qsTr("About the AAC bitrate")
                    Accessible.role: Accessible.Button
                    active: bitrateHelp.visible
                    height: 18
                    width: 18

                    onPressed: bitrateHelp.visible = !bitrateHelp.visible
                }
            }
            Text {
                Layout.fillWidth: true
                color: Theme.textMuted
                font.pixelSize: Theme.fontSizeMd
                text: qsTr("The bitrate only applies to AAC output.")
                wrapMode: Text.WordWrap
            }
            Text {
                id: bitrateHelp

                Layout.fillWidth: true
                color: Theme.textSecondary
                font.pixelSize: Theme.fontSizeMd
                text: qsTr("256 kbps is transparent for most material. Raise it for dense mixes or lower it to save disk space. FLAC and ALAC ignore this value.")
                visible: false
                wrapMode: Text.WordWrap
            }
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: Theme.border
            }
            Text {
                Layout.fillWidth: true
                color: Theme.textPrimary
                font.pixelSize: Theme.fontSizeLg
                font.weight: Theme.fontWeightDemiBold
                text: qsTr("Storage")
            }
            RowLayout {
                Layout.fillWidth: true

                StemsSettingParameter {
                    label: qsTr("Stem files location")
                }
                StemsValueField {
                    Layout.preferredWidth: 280
                    text: root.outputDirectory.length > 0 ? root.outputDirectory : qsTr("Default (%1)").arg(qsTr("Settings folder") + "/Stems")
                }
                Setting.FormButton {
                    activeColor: Theme.textOnAccent
                    backgroundColor: Theme.surface2
                    text: qsTr("Browse…")

                    onPressed: {
                        const directory = Mixxx.Config.chooseDirectory(qsTr("Select stem output directory"), root.outputDirectory);
                        if (directory.length > 0) {
                            root.outputDirectory = directory;
                            root.markDirty();
                        }
                    }
                }
            }
            Text {
                Layout.fillWidth: true
                color: Theme.textMuted
                font.pixelSize: Theme.fontSizeMd
                text: qsTr("Separated stem sets are written to this folder. Leave it empty to use the Stems folder inside the settings directory.")
                wrapMode: Text.WordWrap
            }
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: Theme.border
            }
            Text {
                Layout.fillWidth: true
                color: Theme.textPrimary
                font.pixelSize: Theme.fontSizeLg
                font.weight: Theme.fontWeightDemiBold
                text: qsTr("Separation model")
            }
            RowLayout {
                Layout.fillWidth: true

                StemsSettingParameter {
                    label: qsTr("ONNX model")
                }
                StemsValueField {
                    Layout.preferredWidth: 280
                    text: root.modelPath.length > 0 ? root.modelPath : qsTr("Not configured")
                }
                Setting.FormButton {
                    activeColor: Theme.textOnAccent
                    backgroundColor: Theme.surface2
                    text: qsTr("Browse…")

                    onPressed: {
                        const file = Mixxx.Config.chooseFile(qsTr("Select stem separation model"), root.modelPath, qsTr("ONNX models (*.onnx);;All files (*)"));
                        if (file.length > 0) {
                            root.modelPath = file;
                            root.markDirty();
                        }
                    }
                }
            }
            Text {
                Layout.fillWidth: true
                color: Theme.textMuted
                font.pixelSize: Theme.fontSizeMd
                text: qsTr("Generate Stems uses this ONNX model (for example htdemucs.onnx). Without a model the separation job fails with an explanatory message.")
                wrapMode: Text.WordWrap
            }
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: Theme.border
            }
            Text {
                Layout.fillWidth: true
                color: Theme.textPrimary
                font.pixelSize: Theme.fontSizeLg
                font.weight: Theme.fontWeightDemiBold
                text: qsTr("Performance")
            }
            RowLayout {
                Layout.fillWidth: true

                StemsSettingParameter {
                    label: qsTr("GPU acceleration")
                }
                RatioChoice {
                    id: gpuAccelerationInput

                    options: ["on", "off"]

                    onSelectedChanged: root.markDirty()
                }
            }
            Text {
                Layout.fillWidth: true
                color: Theme.textMuted
                font.pixelSize: Theme.fontSizeMd
                text: qsTr("Stem separation uses the GPU when one is available, which is usually several times faster than the CPU. Turn this off to keep the GPU free for waveforms or other applications.")
                wrapMode: Text.WordWrap
            }
            Text {
                Layout.fillWidth: true
                color: Theme.textSecondary
                font.pixelSize: Theme.fontSizeMd
                text: Mixxx.Config.stemGpuAccelerationDefault
                      ? qsTr("A suitable GPU was detected on this computer, so acceleration is on unless you turn it off.")
                      : qsTr("No suitable GPU was detected, so acceleration is off by default. Turning it on is safe: the separator falls back to the CPU if the GPU cannot run the model.")
                wrapMode: Text.WordWrap
            }
        }
    }
    Item {
        id: buttonActions

        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: 20

        Setting.FormButton {
            activeColor: Theme.textOnAccent
            anchors.left: parent.left
            backgroundColor: Theme.danger
            opacity: enabled ? 1.0 : 0.5
            text: qsTr("Reset")

            onPressed: {
                root.reset();
            }
        }
        Row {
            anchors.right: parent.right
            spacing: 10

            Text {
                id: errorMessage

                Layout.alignment: Qt.AlignVCenter
                Layout.rightMargin: 16
                color: Theme.danger
                text: ""
            }
            Setting.FormButton {
                activeColor: Theme.textOnAccent
                backgroundColor: Theme.surface2
                opacity: enabled ? 1.0 : 0.5
                text: qsTr("Cancel")
                visible: root.dirty

                onPressed: {
                    root.load();
                }
            }
            Setting.FormButton {
                activeColor: Theme.textOnAccent
                backgroundColor: root.dirty ? Theme.accent : Theme.surface2
                enabled: root.dirty
                opacity: enabled ? 1.0 : 0.5
                text: qsTr("Save")

                onPressed: {
                    errorMessage.text = "";
                    root.save();
                }
            }
        }
    }

    component StemsSettingParameter: Mixxx.SettingParameter {
        Layout.fillWidth: true
        implicitHeight: 30

        Text {
            anchors.fill: parent
            color: Theme.textPrimary
            font.pixelSize: Theme.fontSizeMd
            text: parent.label
            verticalAlignment: Text.AlignVCenter
        }
    }
    component StemsValueField: Rectangle {
        property alias text: valueText.text

        Accessible.name: valueText.text
        Accessible.role: Accessible.StaticText
        border.color: Theme.border
        border.width: 1
        color: Theme.surface1
        implicitHeight: 28
        implicitWidth: 280
        opacity: 0.6
        radius: Theme.radiusSm

        Text {
            id: valueText

            anchors.fill: parent
            anchors.leftMargin: Theme.space2
            anchors.rightMargin: Theme.space2
            color: Theme.textSecondary
            elide: Text.ElideMiddle
            font.pixelSize: Theme.fontSizeSm
            verticalAlignment: Text.AlignVCenter
        }
    }
}
