pragma ComponentBehavior: Bound

import "." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import QtQuick.Layouts 1.12
import "Theme"

/// Microphone strip: pregain, PFL, talkover and FX assignments.
Row {
    id: root

    required property int unitNumber
    property int fxUnitCount: 4
    property string group: unitNumber === 1 ? "[Microphone]" : "[Microphone" + unitNumber + "]"
    readonly property bool inputConfigured: inputConfiguredControl.value > 0

    signal fxAssignmentChanged(int unitNumber, bool enabled)

    spacing: Theme.space1

    Loader {
        sourceComponent: root.inputConfigured ? configuredComponent : unconfiguredComponent
    }
    Mixxx.ControlProxy {
        id: inputConfiguredControl

        group: root.group
        key: "input_configured"
    }
    Component {
        id: configuredComponent

        Row {
            layoutDirection: root.layoutDirection
            spacing: root.spacing

            Skin.VuMeter {
                group: root.group
                height: parent.height
                key: "vu_meter"
                width: 4
            }
            Rectangle {
                border.color: Theme.border
                border.width: 1
                color: Theme.surface2
                height: 52
                radius: Theme.radiusSm
                width: 52

                Skin.ControlKnob {
                    anchors.centerIn: parent
                    arcStart: Knob.ArcStart.Minimum
                    color: Theme.accent
                    group: root.group
                    height: parent.height - 2 * Theme.space1
                    key: "pregain"
                    width: height
                }
            }
            Column {
                spacing: Theme.space1

                Skin.SectionText {
                    height: 26
                    text: qsTr("MIC %1").arg(root.unitNumber)
                    width: 52
                }
                Skin.ControlButton {
                    activeColor: Theme.textOnAccent
                    group: root.group
                    key: "pfl"
                    text: qsTr("PFL")
                    toggleable: true
                }
            }
            Skin.EmbeddedBackground {
                height: parent.height
                width: Math.max(56, fxAssignments.implicitWidth)

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0

                    Skin.ControlButton {
                        Layout.fillHeight: true
                        Layout.fillWidth: true
                        activeColor: Theme.textOnAccent
                        group: root.group
                        key: "talkover"
                        text: qsTr("Talk")
                        toggleable: true
                    }
                    RowLayout {
                        id: fxAssignments

                        Layout.fillHeight: true
                        Layout.fillWidth: true
                        spacing: 0

                        Repeater {
                            model: Math.max(0, root.fxUnitCount)

                            Skin.ControlButton {
                                required property int index

                                Layout.fillHeight: true
                                Layout.fillWidth: true
                                activeColor: Theme.textOnAccent
                                group: "[EffectRack1_EffectUnit" + (index + 1) + "]"
                                implicitWidth: 28
                                key: "group_" + root.group + "_enable"
                                text: qsTr("FX%1").arg(index + 1)
                                toggleable: true

                                onHighlightChanged: root.fxAssignmentChanged(index + 1, highlight)
                            }
                        }
                    }
                }
            }
        }
    }
    Component {
        id: unconfiguredComponent

        Column {
            spacing: Theme.space1

            Skin.SectionText {
                color: Theme.textMuted
                height: 21
                text: qsTr("Mic %1").arg(root.unitNumber)
                width: 80
            }
            Skin.ControlButton {
                group: root.group
                key: "talkover"
                text: qsTr("Configure")
                width: 80
            }
        }
    }
}
