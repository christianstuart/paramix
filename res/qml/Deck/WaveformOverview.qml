import QtQuick 2.12
import ".." as Skin
import "../Theme"

/// Flat, bordered overview strip below the deck header.
Rectangle {
    id: overview

    required property var currentTrack
    required property string group

    border.color: Theme.border
    border.width: 1
    clip: true
    color: Theme.surface0
    height: 50
    radius: Theme.radiusSm

    Skin.FadeBehavior on visible {
        fadeTarget: overview
    }

    Skin.WaveformOverview {
        anchors.fill: parent
        group: overview.group
    }
}
