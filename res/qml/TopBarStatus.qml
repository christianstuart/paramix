import Mixxx 1.0 as Mixxx
import QtQuick
import QtQuick.Layouts
import "Theme"

/// System status in the toolbar: CPU, GPU, clock and battery.
///
/// Every item can be switched off in Preferences > Interface > Top bar. The
/// metrics are sampled once per second by the SystemMonitor singleton.
RowLayout {
    id: root

    property string clockText: Qt.formatTime(new Date(), "hh:mm")

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
        visible: Mixxx.Config.paramixShowCpu
    }
    Text {
        id: gpuText

        readonly property real usage: Mixxx.SystemMonitor.gpuUsage

        Accessible.name: qsTr("GPU usage %1 percent").arg(Math.round(usage))
        color: usage >= 95 ? Theme.danger : usage >= 80 ? Theme.warning : Theme.textSecondary
        font.family: Theme.fontFamilyMono
        font.pixelSize: Theme.fontSizeSm
        text: qsTr("GPU %1%").arg(Math.round(usage))
        visible: Mixxx.Config.paramixShowGpu && Mixxx.SystemMonitor.gpuAvailable
    }
    Rectangle {
        Layout.alignment: Qt.AlignVCenter
        color: Theme.borderStrong
        implicitHeight: Math.round(14 * Theme.uiScale)
        implicitWidth: 1
        visible: clock.visible
                 && (cpuText.visible || gpuText.visible)
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

        readonly property real percentage: Math.max(0,
                Math.min(100, Mixxx.Battery.percentage))

        spacing: 3
        visible: Mixxx.Config.paramixShowBattery && Mixxx.Battery.isBatteryAvailable

        Item {
            implicitHeight: Math.round(9 * Theme.uiScale)
            implicitWidth: Math.round(18 * Theme.uiScale)

            Rectangle {
                id: batteryShell

                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.rightMargin: Math.round(2 * Theme.uiScale)
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
                width: Math.max(0, (batteryShell.width - Math.round(4 * Theme.uiScale))
                        * battery.percentage / 100)
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
}
