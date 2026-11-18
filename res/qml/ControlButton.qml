import "." as Skin

Skin.Button {
    id: root

    required property string group
    required property string key
    property bool toggleable: false

    function toggle() {
        controlBehavior.toggleControl();
    }

    highlight: controlBehavior.isActive
    onCanceled: {
        // Qt sends canceled instead of released when the press is stolen by a
        // drag or a focus change; without this a momentary control stays at 1
        // (loop rolls kept looping, held rate nudges stuck).
        controlBehavior.releasePrimary();
    }
    onPressed: {
        controlBehavior.pressPrimary();
    }
    onReleased: {
        controlBehavior.releasePrimary();
    }

    ControlProxyButtonBehavior {
        id: controlBehavior

        group: root.group
        key: root.key
        toggleable: root.toggleable
        handlePointerInput: false
    }
}
