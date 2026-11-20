import QtQuick 2.15
import QtQuick.Layouts
import "../Theme"

/// The last physical message the controller sent, and whether that physical
/// control already has a mapping in the working mapping.
///
/// Presentational only: the window owns the backend calls and passes the
/// `lastMessage` map plus the result of `existingMappingFor()`.
Rectangle {
    id: root

    property var existing: ({})
    property bool learning: false
    property var message: ({})

    readonly property bool hasMessage: root.message && root.message.message !== undefined
    readonly property bool isMapped: root.existing && Object.keys(root.existing).length > 0
    readonly property string existingTitle: {
        if (!root.isMapped)
            return "";
        if (root.existing.title !== undefined && root.existing.title.length > 0)
            return root.existing.title;
        if (root.existing.configKey !== undefined)
            return root.existing.configKey;
        return "";
    }

    Accessible.name: root.hasMessage ? qsTr("Last message %1, %2").arg(root.message.message).arg(root.isMapped ? qsTr("already mapped to %1").arg(root.existingTitle) : qsTr("not mapped yet")) : qsTr("Move a control on the controller")
    Accessible.role: Accessible.Grouping
    border.color: root.isMapped ? Theme.warning : Theme.border
    border.width: 1
    color: Theme.surface1
    implicitHeight: content.implicitHeight + 2 * Theme.space3
    radius: Theme.radiusMd

    ColumnLayout {
        id: content

        anchors.fill: parent
        anchors.margins: Theme.space3
        spacing: Theme.space1

        Text {
            Accessible.role: Accessible.StaticText
            color: Theme.textSecondary
            font.family: Theme.fontFamily
            font.letterSpacing: Theme.letterSpacingCaps
            font.pixelSize: Theme.fontSizeXs
            font.weight: Theme.fontWeightDemiBold
            text: root.hasMessage ? qsTr("LAST MESSAGE") : qsTr("MOVE A CONTROL")
            textFormat: Text.PlainText
        }
        Text {
            Layout.fillWidth: true
            color: root.hasMessage ? Theme.textPrimary : Theme.textMuted
            elide: Text.ElideRight
            font.family: Theme.fontFamilyMono
            font.pixelSize: Theme.fontSizeLg
            font.weight: Theme.fontWeightMedium
            text: root.hasMessage ? root.message.message : (root.learning ? qsTr("Waiting for the controller…") : qsTr("Learning is stopped"))
            textFormat: Text.PlainText
        }
        Text {
            Layout.fillWidth: true
            color: Theme.textSecondary
            elide: Text.ElideRight
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeSm
            text: root.hasMessage ? qsTr("Channel %1 · %2 · control %3 · value %4").arg(root.message.channel + 1).arg(root.message.opCodeName).arg(root.message.control).arg(root.message.value) : ""
            textFormat: Text.PlainText
            visible: text.length > 0
        }
        Text {
            Layout.fillWidth: true
            color: root.isMapped ? Theme.warning : Theme.positive
            elide: Text.ElideRight
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeSm
            font.weight: Theme.fontWeightDemiBold
            text: {
                if (!root.hasMessage)
                    return "";
                if (!root.isMapped)
                    return qsTr("Not mapped yet");
                const path = root.existing.configKey !== undefined ? root.existing.configKey : "";
                return path.length > 0 ? qsTr("Already mapped to %1 (%2)").arg(root.existingTitle).arg(path) : qsTr("Already mapped to %1").arg(root.existingTitle);
            }
            textFormat: Text.PlainText
            visible: text.length > 0
        }
        Text {
            Layout.fillWidth: true
            color: Theme.textMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeXs
            text: root.isMapped ? qsTr("Assigning a new target replaces this mapping.") : qsTr("Pick a target below or click a control in the main window.")
            textFormat: Text.PlainText
            visible: root.hasMessage
            wrapMode: Text.WordWrap
        }
    }
}
