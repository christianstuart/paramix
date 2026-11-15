import "." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick
import QtQuick.Layouts
import "Theme"

/// Global background-task strip, pinned directly below the top toolbar.
///
/// Everything is a binding on `Mixxx.StemService` / `Mixxx.Library`, so the
/// strip appears with the first queued job and collapses again as soon as the
/// last one finishes - no timers, no polling. While idle it is invisible and
/// zero height, so it never reserves layout space.
///
/// Stem separation is rendered first, analysis second. Keeping them as two
/// compact rows means a batch analysis does not hide a separation job.
Rectangle {
    id: root

    /// True while the stem service has any pending job (queued or running).
    readonly property bool stemPending: Mixxx.StemService.busy
            || Mixxx.StemService.queuedCount > 0
    /// True only for the job that is actually running. The service clears
    /// `currentTrackId` as soon as a job reaches a terminal state, so the
    /// detailed row disappears with the last progress update.
    readonly property bool stemSeparating: Mixxx.StemService.currentTrackId >= 0
            && Mixxx.StemService.currentTrackTitle.length > 0
    readonly property bool analysisShowing: Mixxx.Library.analysisActive
    readonly property int rowCount: (root.stemPending ? 1 : 0) + (root.analysisShowing ? 1 : 0)

    color: Theme.surface1
    implicitHeight: root.rowCount === 0 ? 0 : root.rowCount === 1 ? 26 : 42
    visible: root.rowCount > 0

    // 1 px lit top edge + hairline bottom, matching the toolbar.
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        color: Theme.topHighlight
        height: 1
    }
    Rectangle {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        color: Theme.borderStrong
        height: 1
    }
    ColumnLayout {
        anchors.bottomMargin: 3
        anchors.fill: parent
        anchors.leftMargin: Theme.space3
        anchors.rightMargin: Theme.space3
        anchors.topMargin: 3
        spacing: 0

        RowLayout {
            id: stemRow

            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.minimumHeight: 16
            spacing: Theme.space2
            visible: root.stemPending

            Text {
                Layout.fillWidth: true
                color: Theme.textPrimary
                elide: Text.ElideRight
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeSm
                text: root.stemSeparating
                      ? qsTr("Separating stems — %1").arg(Mixxx.StemService.currentTrackTitle)
                      : (Mixxx.StemService.queuedCount === 1
                         ? qsTr("1 stem job queued")
                         : qsTr("%1 stem jobs queued").arg(Mixxx.StemService.queuedCount))
                verticalAlignment: Text.AlignVCenter
            }
            JobBar {
                Layout.alignment: Qt.AlignVCenter
                Layout.preferredWidth: 140
                label: qsTr("Stem separation progress")
                value: Mixxx.StemService.progress
                visible: root.stemSeparating
            }
            Text {
                Layout.preferredWidth: 32
                color: Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeXs
                horizontalAlignment: Text.AlignRight
                text: qsTr("%1%").arg(Math.round(Math.max(0,
                                                           Math.min(100,
                                                                    Mixxx.StemService.progress))))
                verticalAlignment: Text.AlignVCenter
                visible: root.stemSeparating
            }
            Text {
                color: Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeXs
                text: qsTr("+%1 queued").arg(Mixxx.StemService.queuedCount)
                verticalAlignment: Text.AlignVCenter
                visible: root.stemSeparating && Mixxx.StemService.queuedCount > 0
            }
            Skin.Button {
                Layout.alignment: Qt.AlignVCenter
                Accessible.name: qsTr("Cancel stem separation")
                implicitHeight: 18
                text: qsTr("Cancel")

                onClicked: Mixxx.StemService.cancelAll()
            }
        }
        RowLayout {
            id: analysisRow

            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.minimumHeight: 16
            spacing: Theme.space2
            visible: root.analysisShowing

            Text {
                Layout.fillWidth: true
                color: Theme.textPrimary
                elide: Text.ElideRight
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeSm
                text: Mixxx.Library.analysisTrackTitle.length > 0
                      ? qsTr("Analyzing — %1").arg(Mixxx.Library.analysisTrackTitle)
                      : qsTr("Analyzing audio…")
                verticalAlignment: Text.AlignVCenter
            }
            JobBar {
                Layout.alignment: Qt.AlignVCenter
                Layout.preferredWidth: 140
                label: qsTr("Track analysis progress")
                value: Mixxx.Library.analysisProgress
            }
            Text {
                color: Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeXs
                text: qsTr("%1/%2").arg(Mixxx.Library.analysisCurrent).arg(Mixxx.Library.analysisTotal)
                verticalAlignment: Text.AlignVCenter
                visible: Mixxx.Library.analysisTotal > 1
            }
        }
    }

    /// Determinate 0..100 bar: accent fill on a surface2 track. The accessible
    /// role carries the meaning, the bar itself is not interactive.
    component JobBar: Item {
        id: bar

        property string label: ""
        property int value: 0

        Accessible.name: bar.label
        Accessible.role: Accessible.ProgressBar
        implicitHeight: 5
        implicitWidth: 140

        Rectangle {
            anchors.fill: parent
            color: Theme.surface2
            radius: Theme.radiusXs
        }
        Rectangle {
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.top: parent.top
            color: Theme.accent
            radius: Theme.radiusXs
            width: Math.round(parent.width * Math.max(0, Math.min(100, bar.value)) / 100)
        }
    }
}
