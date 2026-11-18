import Mixxx 1.0 as Mixxx
import QtQuick
import QtQuick.Layouts
import "Theme"

/// System status in the toolbar: CPU, GPU, clock, battery, audio buffer headroom
/// and live broadcast state.
///
/// CPU, GPU, clock and battery can be switched off in Preferences > Interface >
/// Top bar. The metrics are sampled once per second by the SystemMonitor
/// singleton; the buffer and broadcast lamps follow their engine controls.
/// Below 1560 px the live routing lamps take the CPU/GPU readouts' place.
RowLayout {
    id: root

    property string clockText: Qt.formatTime(new Date(), "hh:mm")

    /// Below this the top bar cannot hold CPU/GPU plus the live routing lamps
    /// without running into the centered recording box at the larger permitted
    /// font scales, so the live ones win: a dropout or a failed stream matters
    /// more than a diagnostic readout. The cluster keeps roughly the same width
    /// either way.
    readonly property bool compactStatus: Theme.windowWidth < 1560

    spacing: Theme.space2

    Timer {
        interval: 1000
        repeat: true
        running: root.visible
        triggeredOnStart: true

        onTriggered: root.clockText = Qt.formatTime(new Date(), "hh:mm")
    }
    Text {
        id: cpuText

        readonly property real usage: Mixxx.SystemMonitor.cpuUsage

        Accessible.name: qsTr("CPU usage %1 percent").arg(Math.round(usage))
        color: usage >= 95 ? Theme.danger : usage >= 80 ? Theme.warning : Theme.textSecondary
        font.family: Theme.fontFamilyMono
        font.pixelSize: Theme.fontSizeSm
        text: qsTr("CPU %1%").arg(Math.round(usage))
        visible: Mixxx.Config.paramixShowCpu && !root.compactStatus
    }
    Text {
        id: gpuText

        readonly property real usage: Mixxx.SystemMonitor.gpuUsage

        Accessible.name: qsTr("GPU usage %1 percent").arg(Math.round(usage))
        color: usage >= 95 ? Theme.danger : usage >= 80 ? Theme.warning : Theme.textSecondary
        font.family: Theme.fontFamilyMono
        font.pixelSize: Theme.fontSizeSm
        text: qsTr("GPU %1%").arg(Math.round(usage))
        visible: Mixxx.Config.paramixShowGpu && Mixxx.SystemMonitor.gpuAvailable && !root.compactStatus
    }
    Rectangle {
        Layout.alignment: Qt.AlignVCenter
        color: Theme.borderStrong
        implicitHeight: Math.round(14 * Theme.uiScale)
        implicitWidth: 1
        visible: clock.visible && (cpuText.visible || gpuText.visible)
    }
    Text {
        id: clock

        Accessible.name: qsTr("Clock %1").arg(text)
        color: Theme.textSecondary
        font.family: Theme.fontFamilyMono
        font.pixelSize: Theme.fontSizeSm
        text: root.clockText
        visible: Mixxx.Config.paramixShowClock
    }
    RowLayout {
        id: battery

        readonly property real percentage: Math.max(0, Math.min(100, Mixxx.Battery.percentage))

        spacing: 3
        visible: Mixxx.Config.paramixShowBattery && Mixxx.Battery.isBatteryAvailable

        Item {
            implicitHeight: Math.round(9 * Theme.uiScale)
            implicitWidth: Math.round(18 * Theme.uiScale)

            Rectangle {
                id: batteryShell

                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.rightMargin: Math.round(2 * Theme.uiScale)
                anchors.top: parent.top
                border.color: Theme.textMuted
                border.width: 1
                color: "transparent"
                radius: Math.round(1 * Theme.uiScale)
            }
            Rectangle {
                anchors.left: batteryShell.left
                anchors.leftMargin: Math.round(2 * Theme.uiScale)
                anchors.verticalCenter: batteryShell.verticalCenter
                color: Mixxx.Battery.isCharging ? Theme.positive : Theme.textSecondary
                height: Math.max(1, batteryShell.height - Math.round(4 * Theme.uiScale))
                radius: Math.round(0.5 * Theme.uiScale)
                width: Math.max(0, (batteryShell.width - Math.round(4 * Theme.uiScale)) * battery.percentage / 100)
            }
            Rectangle {
                anchors.left: batteryShell.right
                anchors.verticalCenter: batteryShell.verticalCenter
                color: Theme.textMuted
                height: Math.max(2, Math.round(4 * Theme.uiScale))
                radius: Math.round(0.5 * Theme.uiScale)
                width: Math.max(1, Math.round(2 * Theme.uiScale))
            }
        }
        Text {
            Accessible.name: qsTr("Battery %1 percent").arg(Math.round(battery.percentage))
            color: Mixxx.Battery.isCharging ? Theme.positive : Theme.textSecondary
            font.family: Theme.fontFamilyMono
            font.pixelSize: Theme.fontSizeSm
            text: (Mixxx.Battery.isCharging ? "+" : "") + Math.round(battery.percentage) + "%"
        }
    }
    // Audio buffer headroom: a fill bar for the callback time and a lamp that
    // lights while the engine is reporting an underflow.
    RowLayout {
        id: latency

        readonly property bool overload: latencyOverload.value > 0
        readonly property real usage: Math.max(0, Math.min(1, latencyUsage.value))

        Accessible.name: overload ? qsTr("Audio buffer overload") : qsTr("Audio buffer usage %1 percent").arg(Math.round(usage * 100))
        Accessible.role: Accessible.StaticText
        spacing: 3

        Mixxx.ControlProxy {
            id: latencyUsage

            group: "[App]"
            key: "audio_latency_usage"
        }
        Mixxx.ControlProxy {
            id: latencyOverload

            group: "[App]"
            key: "audio_latency_overload"
        }
        Text {
            color: Theme.textMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeXs
            text: qsTr("BUF")
        }
        Item {
            implicitHeight: 6
            implicitWidth: 18

            Rectangle {
                anchors.fill: parent
                border.color: Theme.border
                border.width: 1
                color: Theme.surface3
            }
            Rectangle {
                color: latency.overload || latency.usage >= 0.9 ? Theme.danger : latency.usage >= 0.75 ? Theme.warning : Theme.positive
                height: parent.height - 2
                width: Math.max(0, (parent.width - 2) * latency.usage)
                x: 1
                y: 1
            }
        }
        Rectangle {
            color: latency.overload ? Theme.danger : Theme.surface3
            implicitHeight: 6
            implicitWidth: 6
            radius: 3
        }
    }
    // Live broadcast state: off, connecting (...), on air, degraded (some
    // destinations failed while others are on air), or failed (all
    // destinations failed). Read-only; the Options menu owns the toggle. The
    // loader keeps the [Shoutcast] controls out of builds without live
    // broadcasting, so they cannot warn about missing controls.
    Loader {
        active: Mixxx.Application.liveBroadcastingAvailable
        visible: active

        sourceComponent: Component {
            RowLayout {
                id: broadcast

                readonly property bool enabled: broadcastEnabled.value > 0
                readonly property color stateColor: !enabled ? Theme.textMuted : status === 3 ? Theme.danger : status === 2 ? Theme.positive : Theme.warning
                readonly property real status: broadcastStatus.value

                Accessible.name: !enabled ? qsTr("Live broadcasting off") : status === 3 ? qsTr("Live broadcasting error") : status === 4 ? qsTr("Live broadcasting degraded") : status === 2 ? qsTr("Live broadcasting on air") : qsTr("Live broadcasting connecting")
                Accessible.role: Accessible.StaticText
                spacing: 3

                Mixxx.ControlProxy {
                    id: broadcastEnabled

                    group: "[Shoutcast]"
                    key: "enabled"
                }
                Mixxx.ControlProxy {
                    id: broadcastStatus

                    group: "[Shoutcast]"
                    key: "status"
                }
                Rectangle {
                    color: broadcast.stateColor
                    implicitHeight: 6
                    implicitWidth: 6
                    radius: 3
                }
                Text {
                    color: broadcast.stateColor
                    font.capitalization: Font.AllUppercase
                    font.family: Theme.fontFamily
                    font.letterSpacing: Theme.letterSpacingCaps
                    font.pixelSize: Theme.fontSizeXs
                    text: !broadcast.enabled ? qsTr("Off") : broadcast.status === 3 ? qsTr("Error") : broadcast.status === 4 ? qsTr("Degraded") : broadcast.status === 2 ? qsTr("On air") : qsTr("...")
                }
            }
        }
    }
}
