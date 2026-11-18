import "." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import QtQml
import QtQuick.Controls
import QtQuick.Window 2.12
import "Theme"

ApplicationWindow {
    id: root

    readonly property bool isMobile: Qt.platform.os === "android" || Qt.platform.os === "ios"
    readonly property int designWidth: 1792
    readonly property int designHeight: 1008

    // The design size assumes a full desktop display. On a scaled-down screen
    // (a 1440x900 panel at 200 % is a 720x426 logical desktop) an unclamped
    // window opens larger than the screen, which clips the toolbar and lets
    // deck panes overflow their columns. Fit the window to what is available.
    readonly property int usableWidth: Math.max(1,
            Screen.desktopAvailableWidth > 0 ? Screen.desktopAvailableWidth : designWidth)
    readonly property int usableHeight: Math.max(1,
            Screen.desktopAvailableHeight > 0 ? Screen.desktopAvailableHeight : designHeight)
    /// The skin lays out cleanly down to Theme.minWindowWidth x minWindowHeight.
    /// A smaller window (a 1366x768 panel, a 150 % scaled 1080p one) shrinks the
    /// whole skin uniformly instead of squeezing bands into each other.
    readonly property real skinScale: isMobile ? 1 : Math.max(0.5, Math.min(1,
            contentItem.width / Theme.minWindowWidth,
            contentItem.height / Theme.minWindowHeight))

    // Keep the design tokens in sync with the persisted customization.
    Binding {
        target: Theme
        property: "scheme"
        value: Mixxx.Config.paramixColorScheme
    }
    Binding {
        target: Theme
        property: "accentOverride"
        value: Mixxx.Config.paramixAccentColor
    }
    Binding {
        target: Theme
        property: "fontFamilyOverride"
        value: Mixxx.Config.paramixFontFamily
    }
    Binding {
        target: Theme
        property: "fontScale"
        value: Mixxx.Config.paramixFontScale
    }
    Binding {
        target: Theme
        property: "uiScale"
        value: Mixxx.Config.paramixUiScale
    }
    Binding {
        target: Theme
        property: "motionScale"
        value: Mixxx.Config.paramixMotionScale
    }
    Binding {
        target: Theme
        property: "windowWidth"
        value: root.width / root.skinScale
    }
    Binding {
        target: Theme
        property: "windowHeight"
        value: root.height / root.skinScale
    }

    color: Theme.backgroundColor
    height: isMobile ? Screen.height : Math.min(designHeight, usableHeight)
    menuBar: content.item ? content.item.menuBar : null
    // The window may go below the layout floor; skinScale covers the gap.
    minimumHeight: isMobile ? 0 : Math.min(Theme.minWindowHeight * 0.75, usableHeight)
    minimumWidth: isMobile ? 0 : Math.min(Theme.minWindowWidth * 0.75, usableWidth)
    visible: true
    width: isMobile ? Screen.width : Math.min(designWidth, usableWidth)

    function updateVisibility() {
        if (!Mixxx.Core.ready) {
            return;
        }
        // On a screen smaller than the design size the window would be as big
        // as the whole desktop with its frame hanging off the edge; maximize.
        root.visibility = Mixxx.Config.configStartInFullscreenKey || isMobile
                ? Window.FullScreen
                : (usableWidth < designWidth || usableHeight < designHeight)
                ? Window.Maximized
                : Window.Windowed;
    }

    Connections {
        target: Mixxx.Core
        function onReadyChanged() {
            root.updateVisibility();
        }
    }

    Component.onCompleted: root.updateVisibility()

    Loader {
        id: content

        // shortcut: popups (menus, combo boxes) keep their unscaled size on a
        // scaled skin; scale the Overlay too if that reads wrong on small screens.
        height: parent.height / root.skinScale
        scale: root.skinScale
        transformOrigin: Item.TopLeft
        width: parent.width / root.skinScale

        active: Mixxx.Core.ready
        asynchronous: true
        onStatusChanged: {
            if (status === Loader.Error) {
                console.error("Failed to load the Paramix main window")
                Qt.quit()
            }
        }
        sourceComponent: Component {
            MainWindow {
                applicationWindow: root
            }
        }
    }
    // Startup splash: the wordmark over the four deck colours, bouncing in a
    // staggered loop until the main window is ready. The dots are the same
    // blue/orange/purple/green used by the toolbar's deck indicators, so the
    // launch screen introduces the palette instead of showing a static logo.
    Rectangle {
        id: splash

        anchors.fill: parent
        color: Theme.backgroundColor
        visible: opacity > 0

        Behavior on opacity {
            NumberAnimation {
                duration: 450
                easing.type: Easing.InOutQuad
            }
        }
        Column {
            id: splashBlock

            anchors.centerIn: parent
            opacity: 0
            spacing: Theme.space4

            NumberAnimation on opacity {
                duration: 700
                easing.type: Easing.OutCubic
                from: 0
                running: true
                to: 1
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                color: Theme.textPrimary
                font.bold: true
                font.family: Theme.fontFamilyDisplay
                font.letterSpacing: 5
                font.pixelSize: Theme.fontSizeLg
                text: qsTr("PARAMIX")
            }
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: Theme.space2

                Repeater {
                    model: [Theme.deckA, Theme.deckB, Theme.deckC, Theme.deckD]

                    Rectangle {
                        required property int index
                        required property var modelData

                        color: modelData
                        height: 10
                        radius: width / 2
                        width: 10
                        y: 0

                        SequentialAnimation on y {
                            loops: Animation.Infinite
                            // The splash stays in the item tree after it fades
                            // out. Stop the decorative loop then, otherwise it
                            // keeps waking the render loop for the whole
                            // session.
                            running: splash.opacity > 0

                            PauseAnimation {
                                duration: index * 110
                            }
                            // InOutSine both ways: zero velocity at the top and
                            // the bottom, so the dot eases through the turn and
                            // settles instead of snapping when the pause starts.
                            NumberAnimation {
                                duration: 420
                                easing.type: Easing.InOutSine
                                to: -9
                            }
                            NumberAnimation {
                                duration: 420
                                easing.type: Easing.InOutSine
                                to: 0
                            }
                            PauseAnimation {
                                duration: 240
                            }
                        }
                        // Matching breath so the dots dim slightly at the floor
                        // and brighten at the top of the arc.
                        SequentialAnimation on opacity {
                            loops: Animation.Infinite
                            running: splash.opacity > 0

                            PauseAnimation {
                                duration: index * 110
                            }
                            NumberAnimation {
                                duration: 420
                                easing.type: Easing.InOutSine
                                to: 1
                            }
                            NumberAnimation {
                                duration: 420
                                easing.type: Easing.InOutSine
                                to: 0.65
                            }
                            PauseAnimation {
                                duration: 240
                            }
                        }
                    }
                }
            }
        }
        states: [
            State {
                when: content.status == Loader.Ready && content.active

                PropertyChanges {
                    target: splash
                    opacity: 0
                }
            }
        ]
    }
}
