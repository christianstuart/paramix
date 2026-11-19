pragma ComponentBehavior: Bound

import "." as Skin
import "Effects" as Effects
import Mixxx 1.0 as Mixxx
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import "Theme"

/// Effect units 3 and 4 in their own window. The top-bar strip is fixed to
/// units 1 and 2; this window reuses Effects/TopBarEffectUnit.qml, so both
/// places share one implementation. The window is closed by default and its
/// open state lives in [Skin],show_4effectunits, the control the toolbar key,
/// the View menu and controllers all drive.
///
/// Geometry persists in [Skin] controls created on first use: the QML UI has no
/// settings object of its own, and creating these through the skin control
/// creator keeps it QML-only (the same approach the deck panes use). A future
/// C++ window-state helper could replace the four controls.
Window {
    id: root

    readonly property int framePadding: Theme.space3
    property bool geometryRestored: false

    /// Restore once, after every persisted geometry control has resolved.
    function restoreGeometry() {
        if (root.geometryRestored ||
                !savedX.initialized || !savedY.initialized ||
                !savedWidth.initialized || !savedHeight.initialized) {
            return;
        }
        root.geometryRestored = true;
        const storedWidth = Math.round(savedWidth.value);
        const storedHeight = Math.round(savedHeight.value);
        root.width = Math.max(root.minimumWidth,
                storedWidth > 0 ? storedWidth : root.minimumWidth);
        root.height = Math.max(root.minimumHeight,
                storedHeight > 0 ? storedHeight : root.minimumHeight);
        if (savedX.value >= 0 && savedY.value >= 0) {
            root.x = Math.round(savedX.value);
            root.y = Math.round(savedY.value);
        }
    }
    function openWindow() {
        root.restoreGeometry();
        if (!root.visible) {
            root.show();
            root.raise();
        }
    }

    color: Theme.surface0
    minimumHeight: layout.implicitHeight + 2 * root.framePadding
    minimumWidth: layout.implicitWidth + 2 * root.framePadding
    title: qsTr("Effect units 3-4")
    visible: false

    onClosing: {
        // A title-bar close has to switch the toolbar key and the View menu
        // item off. Do it here rather than on visibleChanged so quitting the
        // app with the window open keeps the persisted open state.
        if (showUnitsControl.value > 0) {
            showUnitsControl.value = 0;
        }
    }
    onHeightChanged: if (root.geometryRestored) savedHeight.value = root.height
    onWidthChanged: if (root.geometryRestored) savedWidth.value = root.width
    onXChanged: if (root.geometryRestored) savedX.value = root.x
    onYChanged: if (root.geometryRestored) savedY.value = root.y
    Component.onCompleted: {
        root.restoreGeometry();
        if (showUnitsControl.value > 0) {
            root.openWindow();
        }
    }

    ColumnLayout {
        id: layout

        anchors.fill: parent
        anchors.margins: root.framePadding
        spacing: Theme.space2

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space2

            Text {
                Layout.fillWidth: true
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeLg
                font.weight: Theme.fontWeightDemiBold
                text: qsTr("Effect units 3-4")
                verticalAlignment: Text.AlignVCenter
            }
            Skin.Button {
                Accessible.name: qsTr("Close effect units 3-4 window")
                ToolTip.delay: 500
                ToolTip.text: Accessible.name
                ToolTip.visible: hovered
                implicitHeight: Theme.fxControlHeight
                text: qsTr("Close")

                onClicked: root.close()
            }
        }
        Rectangle {
            Layout.fillWidth: true
            color: Theme.border
            implicitHeight: 1
        }
        Effects.TopBarEffectUnit {
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.fxControlHeight
            popupAsWindow: true
            showSuper: true
            unitNumber: 3
        }
        Effects.TopBarEffectUnit {
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.fxControlHeight
            popupAsWindow: true
            showSuper: true
            unitNumber: 4
        }
    }
    Mixxx.ControlProxy {
        id: showUnitsControl

        group: "[Skin]"
        key: "show_4effectunits"
    }
    Connections {
        target: showUnitsControl

        function onValueChanged() {
            if (showUnitsControl.value > 0) {
                root.openWindow();
            } else if (root.visible) {
                root.close();
            }
        }
    }

    // Persisted geometry. The defaults mark "never stored" without colliding
    // with a real position on the primary screen.
    Mixxx.SkinControlCreator {
        id: savedXCreator

        defaultValue: -1
        group: "[Skin]"
        key: "effectunits_window_x"
        persist: true
    }
    Mixxx.SkinControlCreator {
        id: savedYCreator

        defaultValue: -1
        group: "[Skin]"
        key: "effectunits_window_y"
        persist: true
    }
    Mixxx.SkinControlCreator {
        id: savedWidthCreator

        defaultValue: 0
        group: "[Skin]"
        key: "effectunits_window_width"
        persist: true
    }
    Mixxx.SkinControlCreator {
        id: savedHeightCreator

        defaultValue: 0
        group: "[Skin]"
        key: "effectunits_window_height"
        persist: true
    }
    Mixxx.ControlProxy {
        id: savedX

        group: "[Skin]"
        key: savedXCreator.key

        onInitializedChanged: root.restoreGeometry()
    }
    Mixxx.ControlProxy {
        id: savedY

        group: "[Skin]"
        key: savedYCreator.key

        onInitializedChanged: root.restoreGeometry()
    }
    Mixxx.ControlProxy {
        id: savedWidth

        group: "[Skin]"
        key: savedWidthCreator.key

        onInitializedChanged: root.restoreGeometry()
    }
    Mixxx.ControlProxy {
        id: savedHeight

        group: "[Skin]"
        key: savedHeightCreator.key

        onInitializedChanged: root.restoreGeometry()
    }
}
