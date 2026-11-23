import Qt5Compat.GraphicalEffects
import QtQuick 2
import QtQuick.Controls 2
import "Theme"

/// Flat, token-driven hardware key. Rekordbox-style: raised face, hairline
/// frame, lit top edge; active state fills with the signal-blue accent.
AbstractButton {
    id: root

    property color activeColor: Theme.textOnAccent
    property bool flat: false
    property bool highlight: false
    /// Side margin of the text label; dense key rows lower it.
    property real labelPadding: 6
    /// Label text size; dense rows raise it when their keys are tall enough.
    property int labelPixelSize: Theme.fontSizeXs
    /// Width the label needs without eliding; dense rows use it as a minimum.
    readonly property real labelWidth: label.implicitWidth + 2 * root.labelPadding
    property color normalColor: Theme.textSecondary
    property color pressedColor: Theme.textPrimary

    readonly property bool activeState: root.checked || root.highlight
    readonly property color labelColor: root.activeState
            ? root.activeColor
            : root.pressed
              ? root.pressedColor
              : root.hovered
                ? Theme.textPrimary
                : root.normalColor

    Accessible.role: Accessible.Button
    Accessible.name: root.text

    hoverEnabled: true
    implicitHeight: 26
    // Auto-fit the label so toolbar text never elides at larger font scales;
    // contentWidth is the text's natural width and doesn't depend on layout.
    implicitWidth: Math.max(56, label.contentWidth + 12)
    opacity: root.enabled ? 1.0 : 0.45

    background: Rectangle {
        readonly property bool showFrame: !root.flat || root.activeState || root.hovered || root.activeFocus

        border.color: root.activeState
                ? Qt.lighter(Theme.accent, 1.12)
                : root.activeFocus
                  ? Theme.focusRing
                  : root.hovered
                    ? Theme.borderStrong
                    : Theme.border
        border.width: showFrame ? 1 : 0
        color: root.activeState
               ? root.pressed
                 ? Theme.accentPressed
                 : Theme.accent
               : root.pressed
                 ? Theme.surface3
                 : root.hovered
                   ? Theme.surfaceHover
                   : root.flat
                     ? "transparent"
                     : Theme.surface2
        radius: Theme.radiusSm

        // Flat key face; active state is a solid fill, no gloss.
        Rectangle {
            anchors.left: parent.left
            anchors.leftMargin: 1
            anchors.right: parent.right
            anchors.rightMargin: 1
            anchors.top: parent.top
            anchors.topMargin: 1
            color: root.activeState ? Qt.rgba(255, 255, 255, 0.08) : "transparent"
            height: 1
            visible: !root.flat
        }

        Behavior on color {
            ColorAnimation {
                duration: Theme.motionFast
            }
        }
        Behavior on border.color {
            ColorAnimation {
                duration: Theme.motionFast
            }
        }
    }
    contentItem: Item {
        Text {
            id: label

            anchors.fill: parent
            anchors.leftMargin: root.labelPadding
            anchors.rightMargin: root.labelPadding
            color: root.labelColor
            elide: Text.ElideRight
            font.capitalization: Font.AllUppercase
            font.family: Theme.fontFamily
            font.letterSpacing: Theme.letterSpacingCaps
            font.pixelSize: root.labelPixelSize
            font.weight: Theme.fontWeightDemiBold
            horizontalAlignment: Text.AlignHCenter
            text: root.text
            verticalAlignment: Text.AlignVCenter
            visible: root.text != null && root.text.length > 0
        }
        Image {
            id: image

            anchors.centerIn: parent
            asynchronous: true
            fillMode: Image.PreserveAspectFit
            height: root.icon.height
            source: root.icon.source
            visible: false
            width: root.icon.width
        }
        ColorOverlay {
            anchors.fill: image
            antialiasing: true
            color: root.labelColor
            source: image
            visible: root.icon.source != null && root.icon.source != ""
        }
    }
}
