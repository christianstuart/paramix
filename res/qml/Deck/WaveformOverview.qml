import QtQuick 2.12
import ".." as Skin
import "../Theme"

/// Overview strip below the deck header.
///
/// Console display surface: recessed well, strong hairline, waveform only.
Rectangle {
    id: overview

    required property var currentTrack
    required property string group

    border.color: Theme.borderStrong
    border.width: 1
    clip: true
    color: Theme.surface0
    height: 50
    radius: Theme.radiusSm

    Rectangle {
        anchors.left: parent.left
        anchors.leftMargin: 1
        anchors.right: parent.right
        anchors.rightMargin: 1
        anchors.top: parent.top
        anchors.topMargin: 1
        color: Theme.topHighlight
        height: 1
    }

    Skin.FadeBehavior on visible {
        fadeTarget: overview
    }

    Skin.WaveformOverview {
        anchors.fill: parent
        group: overview.group
    }
}
