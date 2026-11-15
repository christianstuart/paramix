import Mixxx 1.0 as Mixxx
import QtQml
import QtQuick
import QtQuick.Shapes

Item {
    id: root

    enum ArcStart {
        Minimum,
        Center,
        Maximum
    }

    property real angle: 130
    property bool arc: false
    property color arcColor: "transparent"
    property real arcOffsetX: 0
    property real arcOffsetY: 0
    property real arcRadius: width / 2
    property int arcStart: Knob.Center
    readonly property real arcStartValue: {
        switch (arcStart) {
        case Knob.ArcStart.Minimum:
            return min;
        case Knob.ArcStart.Maximum:
            return max;
        default:
            return valueCenter;
        }
    }
    property int arcStyle: ShapePath.SolidLine
    property var arcStylePattern: []
    property real arcWidth: 2
    property alias background: background.data
    property alias foreground: foreground.data
    property bool hideCursorWhileDragging: true
    property real knobCenterOffsetX: 0
    property real knobCenterOffsetY: 0
    property real max: 1
    property real min: 0
    property real value: min
    readonly property real valueCenter: (max - min) / 2
    // QWheelEvent::angleDelta() reports one standard mouse-wheel step as 120.
    readonly property int wheelDeltaStep: 120
    property real wheelStepSize: (root.max - root.min) / root.wheelSteps
    // Match the legacy MIDI-style knob resolution across the control range.
    readonly property int wheelSteps: 127

    signal turned(real value)

    function angleFrom(targetValue) {
        return targetValue * 2 * root.angle;
    }

    // Only properties available in the minimum supported Qt (6.4) may be used
    // here. Accessible.value/minimumValue/maximumValue/stepSize and the
    // increase/decrease actions were added in Qt 6.5 and make the whole
    // component fail to load on Qt 6.4, which is what Linux CI builds against.
    Accessible.role: Accessible.Dial

    Item {
        id: background

        anchors.fill: parent
    }
    Item {
        id: foreground

        anchors.fill: parent

        transform: Rotation {
            angle: root.angleFrom(root.value - root.valueCenter)
            origin.x: root.width / 2 + root.knobCenterOffsetX
            origin.y: root.height / 2 + root.knobCenterOffsetY
        }
    }
    // The value arc is drawn with Canvas rather than Shape: QtQuick Shapes
    // tessellates the arc into straight segments (which looked jagged and
    // flattened near the top of the circle) unless multisampling is enabled
    // globally, and that preference defaults to Disabled. The 2D canvas draws
    // a genuinely circular, anti-aliased arc at any size.
    Canvas {
        id: arcCanvas

        anchors.fill: parent
        antialiasing: true
        renderStrategy: Canvas.Immediate
        visible: root.arc

        onHeightChanged: requestPaint()
        onPaint: {
            const ctx = getContext("2d");
            ctx.reset();
            ctx.clearRect(0, 0, width, height);
            if (!root.arc) {
                return;
            }
            const centerX = width / 2 + root.arcOffsetX;
            const centerY = height / 2 + root.arcOffsetY;
            const startDegrees = root.angleFrom(root.arcStartValue - root.valueCenter) - 90;
            const sweepDegrees = root.angleFrom(root.value - root.arcStartValue);
            const startRadians = startDegrees * Math.PI / 180;
            const endRadians = (startDegrees + sweepDegrees) * Math.PI / 180;

            ctx.lineWidth = root.arcWidth;
            ctx.strokeStyle = root.arcColor;
            ctx.lineCap = "butt";
            ctx.setLineDash(root.arcStyle === ShapePath.DashLine ? root.arcStylePattern : []);
            ctx.beginPath();
            ctx.arc(centerX, centerY, Math.max(0, root.arcRadius), startRadians, endRadians, sweepDegrees < 0);
            ctx.stroke();
        }
        onWidthChanged: requestPaint()

        Connections {
            target: root

            function onAngleChanged() {
                arcCanvas.requestPaint();
            }
            function onArcColorChanged() {
                arcCanvas.requestPaint();
            }
            function onArcOffsetXChanged() {
                arcCanvas.requestPaint();
            }
            function onArcOffsetYChanged() {
                arcCanvas.requestPaint();
            }
            function onArcRadiusChanged() {
                arcCanvas.requestPaint();
            }
            function onArcStartChanged() {
                arcCanvas.requestPaint();
            }
            function onArcStyleChanged() {
                arcCanvas.requestPaint();
            }
            function onArcStylePatternChanged() {
                arcCanvas.requestPaint();
            }
            function onArcWidthChanged() {
                arcCanvas.requestPaint();
            }
            function onValueChanged() {
                arcCanvas.requestPaint();
            }
        }
    }
    DragHandler {
        id: dragHandler

        property vector2d lastTranslation: Qt.vector2d(0, 0)
        property real value

        acceptedButtons: Qt.LeftButton | Qt.MiddleButton
        target: null

        onActiveChanged: {
            dragHandler.value = root.value;
            lastTranslation = Qt.vector2d(0, 0);
        }
        onTranslationChanged: {
            const diff_x = (translation.x - lastTranslation.x);
            const diff_y = (translation.y - lastTranslation.y);
            this.lastTranslation = translation;
            const y_dominant = Math.abs(diff_y) > Math.abs(diff_x);
            let dist = Math.sqrt(diff_x * diff_x + diff_y * diff_y);
            // If y is dominant, then treat an increase in dy as negative (y is
            // pointed downward). Otherwise, if y is not dominant and x has
            // decreased, then treat it as negative.
            if ((y_dominant && diff_y > 0) || (!y_dominant && diff_x < 0))
                dist = -dist;

            let value = root.value + dist / root.wheelSteps;
            // Clamp to [0.0, 1.0].
            value = Mixxx.MathUtils.clamp(value, 0, 1);
            root.turned(value);
            dragHandler.value = value;
        }
    }
    Binding {
        property: "value"
        target: root
        value: dragHandler.value
        when: dragHandler.active
    }
    PointHandler {
        id: pointerHandler

        acceptedButtons: Qt.LeftButton | Qt.MiddleButton
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
        enabled: root.hideCursorWhileDragging && root.Window.active
    }
    MouseArea {
        acceptedButtons: Qt.NoButton
        anchors.fill: parent
        anchors.margins: -Math.max(Screen.desktopAvailableWidth, Screen.desktopAvailableHeight)
        cursorShape: Qt.BlankCursor
        enabled: root.enabled && root.visible && pointerHandler.enabled && pointerHandler.active
        hoverEnabled: true
        parent: root.Window.window ? root.Window.window.contentItem : null
        visible: enabled
        z: 2147483647
    }
    WheelHandler {
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
        target: null

        onWheel: event => {
            const wheelAdjustment = event.angleDelta.y / root.wheelDeltaStep * root.wheelStepSize;
            const value = Mixxx.MathUtils.clamp(root.value + wheelAdjustment, root.min, root.max);
            root.turned(value);
            event.accepted = true;
        }
    }
}
